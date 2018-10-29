// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flash_ui.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/crash_upload_list/crash_upload_list.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/webplugininfo.h"
#include "gpu/config/gpu_info.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using base::ASCIIToUTF16;
using base::UserMetricsAction;
using content::GpuDataManager;
using content::PluginService;
using content::WebContents;
using content::WebUIMessageHandler;
using webui::webui_util::AddPair;

namespace {

const char kFlashPlugin[] = "Flash plugin";

content::WebUIDataSource* CreateFlashUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIFlashHost);
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");

  source->AddLocalizedString("loadingMessage", IDS_FLASH_LOADING_MESSAGE);
  source->AddLocalizedString("flashLongTitle", IDS_FLASH_TITLE_MESSAGE);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("about_flash.js", IDR_ABOUT_FLASH_JS);
  source->SetDefaultResource(IDR_ABOUT_FLASH_HTML);
  return source;
}

const int kTimeout = 8 * 1000;  // 8 seconds.

////////////////////////////////////////////////////////////////////////////////
//
// FlashDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for JavaScript messages for the about:flags page.
class FlashDOMHandler : public WebUIMessageHandler,
                        public content::GpuDataManagerObserver {
 public:
  FlashDOMHandler();
  ~FlashDOMHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // GpuDataManager::Observer implementation.
  void OnGpuInfoUpdate() override;

  // Callback for the "requestFlashInfo" message.
  void HandleRequestFlashInfo(const base::ListValue* args);

  // Callback for the Flash plugin information.
  void OnGotPlugins(const std::vector<content::WebPluginInfo>& plugins);

 private:
  // UploadList callback.
  void OnUploadListAvailable();

  // Called when we think we might have enough information to return data back
  // to the page.
  void MaybeRespondToPage();

  // In certain cases we might not get called back from the GPU process so we
  // set an upper limit on the time we wait. This function gets called when the
  // time has passed. This actually doesn't prevent the rest of the information
  // to appear later, the page will just reflow when more information becomes
  // available.
  void OnTimeout();

  // A timer to keep track of when the data fetching times out.
  base::OneShotTimer timeout_;

  // Crash list.
  scoped_refptr<UploadList> upload_list_;

  // Whether the list of all crashes is available.
  bool crash_list_available_;
  // Whether the page has requested data.
  bool page_has_requested_data_;
  // Whether the GPU data has been collected.
  bool has_gpu_info_;
  // Whether the plugin information is ready.
  bool has_plugin_info_;

  base::WeakPtrFactory<FlashDOMHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FlashDOMHandler);
};

FlashDOMHandler::FlashDOMHandler()
    : crash_list_available_(false),
      page_has_requested_data_(false),
      has_gpu_info_(false),
      has_plugin_info_(false),
      weak_ptr_factory_(this) {
  // Request Crash data asynchronously.
  upload_list_ = CreateCrashUploadList();
  upload_list_->Load(base::BindOnce(&FlashDOMHandler::OnUploadListAvailable,
                                    weak_ptr_factory_.GetWeakPtr()));

  // Watch for changes in GPUInfo.
  GpuDataManager::GetInstance()->AddObserver(this);

  // Tell GpuDataManager it should have full GpuInfo. If the
  // GPU process has not run yet, this will trigger its launch.
  GpuDataManager::GetInstance()->RequestCompleteGpuInfoIfNeeded();

  // GPU access might not be allowed at all, which will cause us not to
  // get a call back.
  if (!GpuDataManager::GetInstance()->GpuAccessAllowed(NULL))
    OnGpuInfoUpdate();

  PluginService::GetInstance()->GetPlugins(base::Bind(
      &FlashDOMHandler::OnGotPlugins, weak_ptr_factory_.GetWeakPtr()));

  // And lastly, we fire off a timer to make sure we never get stuck at
  // the "Loading..." message.
  timeout_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kTimeout),
                 this, &FlashDOMHandler::OnTimeout);
}

FlashDOMHandler::~FlashDOMHandler() {
  GpuDataManager::GetInstance()->RemoveObserver(this);
  upload_list_->CancelCallback();
}

void FlashDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestFlashInfo",
      base::BindRepeating(&FlashDOMHandler::HandleRequestFlashInfo,
                          base::Unretained(this)));
}

void FlashDOMHandler::OnUploadListAvailable() {
  crash_list_available_ = true;
  MaybeRespondToPage();
}

void AddPair(base::ListValue* list,
             const base::string16& key,
             const base::string16& value) {
  std::unique_ptr<base::DictionaryValue> results(new base::DictionaryValue());
  results->SetString("key", key);
  results->SetString("value", value);
  list->Append(std::move(results));
}

void AddPair(base::ListValue* list,
             const base::string16& key,
             const std::string& value) {
  AddPair(list, key, ASCIIToUTF16(value));
}

void FlashDOMHandler::HandleRequestFlashInfo(const base::ListValue* args) {
  page_has_requested_data_ = true;
  MaybeRespondToPage();
}

void FlashDOMHandler::OnGpuInfoUpdate() {
  has_gpu_info_ = true;
  MaybeRespondToPage();
}

void FlashDOMHandler::OnGotPlugins(
    const std::vector<content::WebPluginInfo>& plugins) {
  has_plugin_info_ = true;
  MaybeRespondToPage();
}

void FlashDOMHandler::OnTimeout() {
  // We don't set page_has_requested_data_ because that is guaranteed to appear
  // and we shouldn't be responding to the page before then.
  has_gpu_info_ = true;
  crash_list_available_ = true;
  has_plugin_info_ = true;
  MaybeRespondToPage();
}

void FlashDOMHandler::MaybeRespondToPage() {
  // We don't reply until everything is ready. The page is showing a 'loading'
  // message until then. If you add criteria to this list, please update the
  // function OnTimeout() as well.
  if (!page_has_requested_data_ || !crash_list_available_ || !has_gpu_info_ ||
      !has_plugin_info_) {
    return;
  }

  timeout_.Stop();

  // This is code that runs only when the user types in about:flash. We don't
  // need to jump through hoops to offload this to the IO thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  auto list = std::make_unique<base::ListValue>();

  // Chrome version information.
  AddPair(
      list.get(), l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      version_info::GetVersionNumber() + " (" + chrome::GetChannelName() + ")");

  // OS version information.
  std::string os_label = version_info::GetOSType();
#if defined(OS_WIN)
  base::win::OSInfo* os = base::win::OSInfo::GetInstance();
  switch (os->version()) {
    case base::win::VERSION_XP: os_label += " XP"; break;
    case base::win::VERSION_SERVER_2003:
      os_label += " Server 2003 or XP Pro 64 bit";
      break;
    case base::win::VERSION_VISTA: os_label += " Vista or Server 2008"; break;
    case base::win::VERSION_WIN7: os_label += " 7 or Server 2008 R2"; break;
    case base::win::VERSION_WIN8: os_label += " 8 or Server 2012"; break;
    default:  os_label += " UNKNOWN"; break;
  }
  os_label += " SP" + base::IntToString(os->service_pack().major);
  if (os->service_pack().minor > 0)
    os_label += "." + base::IntToString(os->service_pack().minor);
  if (os->architecture() == base::win::OSInfo::X64_ARCHITECTURE)
    os_label += " 64 bit";
#endif
  AddPair(list.get(), l10n_util::GetStringUTF16(IDS_VERSION_UI_OS), os_label);

  // Obtain the version of the Flash plugins.
  std::vector<content::WebPluginInfo> info_array;
  PluginService::GetInstance()->GetPluginInfoArray(
      GURL(), content::kFlashPluginSwfMimeType, false, &info_array, NULL);
  if (info_array.empty()) {
    AddPair(list.get(), ASCIIToUTF16(kFlashPlugin), "Not installed");
  } else {
    PluginPrefs* plugin_prefs =
        PluginPrefs::GetForProfile(Profile::FromWebUI(web_ui())).get();
    bool found_enabled = false;
    for (size_t i = 0; i < info_array.size(); ++i) {
      base::string16 flash_version = info_array[i].version + ASCIIToUTF16(" ") +
                               info_array[i].path.LossyDisplayName();
      if (plugin_prefs->IsPluginEnabled(info_array[i])) {
        // If we have already found an enabled Flash version, this one
        // is not used.
        if (found_enabled)
          flash_version += ASCIIToUTF16(" (not used)");

        found_enabled = true;
      } else {
        flash_version += ASCIIToUTF16(" (disabled)");
      }
      AddPair(list.get(), ASCIIToUTF16(kFlashPlugin), flash_version);
    }
  }

  // Crash information.
  AddPair(list.get(), base::string16(), "--- Crash data ---");
  bool crash_reporting_enabled =
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
  if (crash_reporting_enabled) {
    std::vector<UploadList::UploadInfo> crashes;
    upload_list_->GetUploads(10, &crashes);

    for (auto i = crashes.begin(); i != crashes.end(); ++i) {
      base::string16 crash_string(ASCIIToUTF16(i->upload_id));
      crash_string += ASCIIToUTF16(" ");
      crash_string += base::TimeFormatFriendlyDateAndTime(i->upload_time);
      AddPair(list.get(), ASCIIToUTF16("crash id"), crash_string);
    }
  } else {
    AddPair(list.get(), ASCIIToUTF16("Crash Reporting"),
            "Enable crash reporting to see crash IDs");
    AddPair(list.get(), ASCIIToUTF16("For more details"),
            chrome::kLearnMoreReportingURL);
  }

  // GPU information.
  AddPair(list.get(), base::string16(), "--- GPU information ---");
  const gpu::GPUInfo gpu_info = GpuDataManager::GetInstance()->GetGPUInfo();
  const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();

  std::string reason;
  if (!GpuDataManager::GetInstance()->GpuAccessAllowed(&reason)) {
    AddPair(list.get(), ASCIIToUTF16("WARNING:"),
            "GPU access is not allowed: " + reason);
  }
#if defined(OS_WIN)
  const gpu::DxDiagNode& node = gpu_info.dx_diagnostics;
  for (std::map<std::string, gpu::DxDiagNode>::const_iterator it =
           node.children.begin();
       it != node.children.end();
       ++it) {
    for (std::map<std::string, std::string>::const_iterator it2 =
             it->second.values.begin();
         it2 != it->second.values.end();
         ++it2) {
      if (!it2->second.empty()) {
        if (it2->first == "szDescription") {
          AddPair(list.get(), ASCIIToUTF16("Graphics card"), it2->second);
        } else if (it2->first == "szDriverNodeStrongName") {
          AddPair(list.get(), ASCIIToUTF16("Driver name (strong)"),
                  it2->second);
        } else if (it2->first == "szDriverName") {
          AddPair(list.get(), ASCIIToUTF16("Driver display name"), it2->second);
        }
      }
    }
  }
#endif

  AddPair(list.get(), base::string16(), "--- GPU driver, more information ---");
  AddPair(list.get(), ASCIIToUTF16("Vendor Id"),
          base::StringPrintf("0x%04x", active_gpu.vendor_id));
  AddPair(list.get(), ASCIIToUTF16("Device Id"),
          base::StringPrintf("0x%04x", active_gpu.device_id));
  AddPair(list.get(), ASCIIToUTF16("Driver vendor"), active_gpu.driver_vendor);
  AddPair(list.get(), ASCIIToUTF16("Driver version"),
          active_gpu.driver_version);
  AddPair(list.get(), ASCIIToUTF16("Driver date"), active_gpu.driver_date);
  AddPair(list.get(), ASCIIToUTF16("Pixel shader version"),
          gpu_info.pixel_shader_version);
  AddPair(list.get(), ASCIIToUTF16("Vertex shader version"),
          gpu_info.vertex_shader_version);
  AddPair(list.get(), ASCIIToUTF16("GL_VENDOR"), gpu_info.gl_vendor);
  AddPair(list.get(), ASCIIToUTF16("GL_RENDERER"), gpu_info.gl_renderer);
  AddPair(list.get(), ASCIIToUTF16("GL_VERSION"), gpu_info.gl_version);
  AddPair(list.get(), ASCIIToUTF16("GL_EXTENSIONS"), gpu_info.gl_extensions);

  base::DictionaryValue flashInfo;
  flashInfo.Set("flashInfo", std::move(list));
  web_ui()->CallJavascriptFunctionUnsafe("returnFlashInfo", flashInfo);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// FlashUI
//
///////////////////////////////////////////////////////////////////////////////

FlashUI::FlashUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  base::RecordAction(UserMetricsAction("ViewAboutFlash"));

  web_ui->AddMessageHandler(std::make_unique<FlashDOMHandler>());

  // Set up the about:flash source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateFlashUIHTMLSource());
}

// static
base::RefCountedMemory* FlashUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  // Use the default icon for now.
  return NULL;
}
