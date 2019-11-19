// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/crashes_ui.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/crash_upload_list/crash_upload_list.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "components/crash/core/browser/crashes_ui_util.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#endif

#if defined(OS_LINUX)
#include "components/crash/content/app/crashpad.h"
#endif

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateCrashesUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUICrashesHost);

  for (size_t i = 0; i < crash_reporter::kCrashesUILocalizedStringsCount; ++i) {
    source->AddLocalizedString(
        crash_reporter::kCrashesUILocalizedStrings[i].name,
        crash_reporter::kCrashesUILocalizedStrings[i].resource_id);
  }

  source->AddLocalizedString(crash_reporter::kCrashesUIShortProductName,
                             IDS_SHORT_PRODUCT_NAME);
  source->UseStringsJs();
  source->AddResourcePath(crash_reporter::kCrashesUICrashesJS,
                          IDR_CRASH_CRASHES_JS);
  source->SetDefaultResource(IDR_CRASH_CRASHES_HTML);
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// CrashesDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the chrome://crashes/ page.
class CrashesDOMHandler : public WebUIMessageHandler {
 public:
  CrashesDOMHandler();
  ~CrashesDOMHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void OnUploadListAvailable();

  // Asynchronously fetches the list of crashes. Called from JS.
  void HandleRequestCrashes(const base::ListValue* args);

#if defined(OS_CHROMEOS)
  // Asynchronously triggers crash uploading. Called from JS.
  void HandleRequestUploads(const base::ListValue* args);
#endif

  // Sends the recent crashes list JS.
  void UpdateUI();

  // Asynchronously requests a user triggered upload. Called from JS.
  void HandleRequestSingleCrashUpload(const base::ListValue* args);

  scoped_refptr<UploadList> upload_list_;
  bool list_available_;
  bool first_load_;

  DISALLOW_COPY_AND_ASSIGN(CrashesDOMHandler);
};

CrashesDOMHandler::CrashesDOMHandler()
    : list_available_(false), first_load_(true) {
  upload_list_ = CreateCrashUploadList();
}

CrashesDOMHandler::~CrashesDOMHandler() {
  upload_list_->CancelLoadCallback();
}

void CrashesDOMHandler::RegisterMessages() {
  upload_list_->Load(base::BindOnce(&CrashesDOMHandler::OnUploadListAvailable,
                                    base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      crash_reporter::kCrashesUIRequestCrashList,
      base::BindRepeating(&CrashesDOMHandler::HandleRequestCrashes,
                          base::Unretained(this)));

#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      crash_reporter::kCrashesUIRequestCrashUpload,
      base::BindRepeating(&CrashesDOMHandler::HandleRequestUploads,
                          base::Unretained(this)));
#endif

  web_ui()->RegisterMessageCallback(
      crash_reporter::kCrashesUIRequestSingleCrashUpload,
      base::BindRepeating(&CrashesDOMHandler::HandleRequestSingleCrashUpload,
                          base::Unretained(this)));
}

void CrashesDOMHandler::HandleRequestCrashes(const base::ListValue* args) {
  if (first_load_) {
    first_load_ = false;
    if (list_available_)
      UpdateUI();
  } else {
    list_available_ = false;
    upload_list_->Load(base::BindOnce(&CrashesDOMHandler::OnUploadListAvailable,
                                      base::Unretained(this)));
  }
}

#if defined(OS_CHROMEOS)
void CrashesDOMHandler::HandleRequestUploads(const base::ListValue* args) {
  chromeos::DebugDaemonClient* debugd_client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  DCHECK(debugd_client);

  debugd_client->UploadCrashes();
}
#endif

void CrashesDOMHandler::OnUploadListAvailable() {
  list_available_ = true;
  if (!first_load_)
    UpdateUI();
}

void CrashesDOMHandler::UpdateUI() {
  bool crash_reporting_enabled =
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();

  bool system_crash_reporter = false;
#if defined(OS_CHROMEOS)
  // Chrome OS has a system crash reporter.
  system_crash_reporter = true;
#endif

  bool using_crashpad = false;
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_ANDROID)
  using_crashpad = true;
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // ChromeOS uses crash_sender instead of Crashpad for uploads even when
  // Crashpad is enabled for dump generation.
  using_crashpad = crash_reporter::IsCrashpadEnabled();
#endif

  bool is_internal = false;
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  if (identity_manager) {
    is_internal = gaia::IsGoogleInternalAccountEmail(
        identity_manager->GetPrimaryAccountInfo().email);
  }

  // Manual uploads currently are supported only for Crashpad-using platforms
  // and only if crash uploads are not disabled by policy.
  bool support_manual_uploads =
      using_crashpad &&
      (crash_reporting_enabled || !IsMetricsReportingPolicyManaged());

  // Show crash reports regardless of |crash_reporting_enabled| when using
  // Crashpad so that users can manually upload those reports.
  bool upload_list = using_crashpad || crash_reporting_enabled;

  base::ListValue crash_list;
  if (upload_list)
    crash_reporter::UploadListToValue(upload_list_.get(), &crash_list);

  base::Value enabled(crash_reporting_enabled);
  base::Value dynamic_backend(system_crash_reporter);
  base::Value manual_uploads(support_manual_uploads);
  base::Value version(version_info::GetVersionNumber());
  base::Value os_string(base::SysInfo::OperatingSystemName() + " " +
                        base::SysInfo::OperatingSystemVersion());
  base::Value is_google_account(is_internal);

  std::vector<const base::Value*> args;
  args.push_back(&enabled);
  args.push_back(&dynamic_backend);
  args.push_back(&manual_uploads);
  args.push_back(&crash_list);
  args.push_back(&version);
  args.push_back(&os_string);
  args.push_back(&is_google_account);
  web_ui()->CallJavascriptFunctionUnsafe(
      crash_reporter::kCrashesUIUpdateCrashList, args);
}

void CrashesDOMHandler::HandleRequestSingleCrashUpload(
    const base::ListValue* args) {
  DCHECK(args);

  std::string local_id;
  bool success = args->GetString(0, &local_id);
  DCHECK(success);

  // Only allow manual uploads if crash uploads aren’t disabled by policy.
  if (!ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled() &&
      IsMetricsReportingPolicyManaged()) {
    return;
  }
  upload_list_->RequestSingleUploadAsync(local_id);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// CrashesUI
//
///////////////////////////////////////////////////////////////////////////////

CrashesUI::CrashesUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<CrashesDOMHandler>());

  // Set up the chrome://crashes/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateCrashesUIHTMLSource());
}

// static
base::RefCountedMemory* CrashesUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_CRASH_SAD_FAVICON, scale_factor);
}
