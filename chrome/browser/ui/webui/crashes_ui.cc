// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/crashes_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/crash_upload_list/crash_upload_list.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/crash/core/browser/crashes_ui_util.h"
#include "components/grit/components_scaled_resources.h"
#include "components/grit/dev_ui_components_resources.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/crash/core/app/crashpad.h"
#endif

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

void CreateAndAddCrashesUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUICrashesHost);

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
  source->AddResourcePath(crash_reporter::kCrashesUICrashesCSS,
                          IDR_CRASH_CRASHES_CSS);
  source->AddResourcePath(crash_reporter::kCrashesUISadTabSVG,
                          IDR_CRASH_SADTAB_SVG);
  source->SetDefaultResource(IDR_CRASH_CRASHES_HTML);
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

  CrashesDOMHandler(const CrashesDOMHandler&) = delete;
  CrashesDOMHandler& operator=(const CrashesDOMHandler&) = delete;

  ~CrashesDOMHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

 private:
  void OnUploadListAvailable();

  // Asynchronously fetches the list of crashes. Called from JS.
  void HandleRequestCrashes(const base::Value::List& args);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Asynchronously triggers crash uploading. Called from JS.
  void HandleRequestUploads(const base::Value::List& args);
#endif

  // Sends the recent crashes list JS.
  void UpdateUI();

  // Asynchronously requests a user triggered upload. Called from JS.
  void HandleRequestSingleCrashUpload(const base::Value::List& args);

  scoped_refptr<UploadList> upload_list_;
  bool list_available_;
  bool first_load_;
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

void CrashesDOMHandler::OnJavascriptDisallowed() {
  upload_list_->CancelLoadCallback();
}

void CrashesDOMHandler::HandleRequestCrashes(const base::Value::List& args) {
  AllowJavascript();
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
void CrashesDOMHandler::HandleRequestUploads(const base::Value::List& args) {
  ash::DebugDaemonClient* debugd_client = ash::DebugDaemonClient::Get();
  DCHECK(debugd_client);

  debugd_client->UploadCrashes(base::BindOnce([](bool success) {
    if (!success) {
      LOG(WARNING) << "crash_sender failed or timed out";
    }
  }));
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
#if BUILDFLAG(IS_CHROMEOS)
  // Chrome OS has a system crash reporter.
  system_crash_reporter = true;
#endif

  bool is_internal = false;
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  if (identity_manager) {
    is_internal = gaia::IsGoogleInternalAccountEmail(
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .email);
  }

  bool manual_uploads_supported = false;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
  manual_uploads_supported = true;
#endif
  bool allow_manual_uploads =
      manual_uploads_supported &&
      (crash_reporting_enabled || !IsMetricsReportingPolicyManaged());

  // Show crash reports regardless of |crash_reporting_enabled| when it is
  // possible to manually upload reports.
  bool upload_list = manual_uploads_supported || crash_reporting_enabled;

  base::Value::List crash_list;
  if (upload_list)
    crash_reporter::UploadListToValue(upload_list_.get(), &crash_list);

  base::Value::Dict result;
  result.Set("enabled", crash_reporting_enabled);
  result.Set("dynamicBackend", system_crash_reporter);
  result.Set("manualUploads", allow_manual_uploads);
  result.Set("crashes", std::move(crash_list));
  result.Set("version", version_info::GetVersionNumber());
  result.Set("os", base::SysInfo::OperatingSystemName() + " " +
                       base::SysInfo::OperatingSystemVersion());
  result.Set("isGoogleAccount", is_internal);
  FireWebUIListener(crash_reporter::kCrashesUIUpdateCrashList, result);
}

void CrashesDOMHandler::HandleRequestSingleCrashUpload(
    const base::Value::List& args) {
  // Only allow manual uploads if crash uploads aren’t disabled by policy.
  if (!ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled() &&
      IsMetricsReportingPolicyManaged()) {
    return;
  }

  std::string local_id = args[0].GetString();
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
  CreateAndAddCrashesUIHTMLSource(Profile::FromWebUI(web_ui));
}

// static
base::RefCountedMemory* CrashesUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_CRASH_SAD_FAVICON, scale_factor);
}
