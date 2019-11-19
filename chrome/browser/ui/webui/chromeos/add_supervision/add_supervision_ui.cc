// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_ui.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/system/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/views/chrome_web_dialog_view.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision.mojom.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_handler_utils.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_metrics_recorder.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/confirm_signout_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace chromeos {

namespace {

constexpr int kDialogHeightPx = 608;
constexpr int kDialogWidthPx = 768;

// Shows the dialog indicating that user has to sign out if supervision has been
// enabled for their account.  Returns a boolean indicating whether the
// ConfirmSignoutDialog is being shown.
bool MaybeShowConfirmSignoutDialog() {
  if (EnrollmentCompleted()) {
    ConfirmSignoutDialog::Show();
    return true;
  }
  return false;
}

const char kAddSupervisionDefaultURL[] =
    "https://families.google.com/supervision/setup";
const char kAddSupervisionFlowType[] = "1";
const char kAddSupervisionSwitch[] = "add-supervision-url";

}  // namespace

// AddSupervisionDialog implementations.

// static
void AddSupervisionDialog::Show(gfx::NativeView parent) {
  // Get the system singleton instance of the AddSupervisionDialog.
  SystemWebDialogDelegate* current_instance = GetInstance();
  if (current_instance) {
    // Focus the dialog if it is already there.  Currently, this is
    // effectively a no-op, since the dialog is system-modal, but
    // it's here nonethless so that if the dialog becomes non-modal
    // at some point, the correct focus behavior occurs.
    current_instance->Focus();
    return;
  }
  // Note:  |current_instance|'s memory is freed when
  // SystemWebDialogDelegate::OnDialogClosed() is called.
  current_instance = new AddSupervisionDialog();

  current_instance->ShowSystemDialogForBrowserContext(
      ProfileManager::GetPrimaryUserProfile(), parent);

  // Record UMA metric that user has initiated the Add Supervision process.
  AddSupervisionMetricsRecorder::GetInstance()->RecordAddSupervisionEnrollment(
      AddSupervisionMetricsRecorder::EnrollmentState::kInitiated);
}

// static
SystemWebDialogDelegate* AddSupervisionDialog::GetInstance() {
  return SystemWebDialogDelegate::FindInstance(
      chrome::kChromeUIAddSupervisionURL);
}

// static
void AddSupervisionDialog::Close() {
  SystemWebDialogDelegate* current_instance = GetInstance();
  if (current_instance) {
    current_instance->Close();
  }
}

void AddSupervisionDialog::CloseNowForTesting() {
  SystemWebDialogDelegate* current_instance = GetInstance();
  if (current_instance) {
    DCHECK(dialog_window()) << "No dialog window instance currently set.";
    views::Widget::GetWidgetForNativeWindow(dialog_window())->CloseNow();
  }
}

ui::ModalType AddSupervisionDialog::GetDialogModalType() const {
  return ui::ModalType::MODAL_TYPE_WINDOW;
}

void AddSupervisionDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidthPx, kDialogHeightPx);
}

bool AddSupervisionDialog::CanCloseDialog() const {
  bool showing_confirm_dialog = MaybeShowConfirmSignoutDialog();
  return !showing_confirm_dialog;
}

bool AddSupervisionDialog::OnDialogCloseRequested() {
  // Record UMA metric that user has closed the Add Supervision dialog.
  AddSupervisionMetricsRecorder::GetInstance()->RecordAddSupervisionEnrollment(
      AddSupervisionMetricsRecorder::EnrollmentState::kClosed);
  return true;
}

AddSupervisionDialog::AddSupervisionDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIAddSupervisionURL),
                              base::string16()) {}

AddSupervisionDialog::~AddSupervisionDialog() = default;

// AddSupervisionUI implementations.

// static
signin::IdentityManager* AddSupervisionUI::test_identity_manager_ = nullptr;

AddSupervisionUI::AddSupervisionUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Register the Mojo API handler.
  AddHandlerToRegistry(base::BindRepeating(
      &AddSupervisionUI::BindAddSupervisionHandler, base::Unretained(this)));

  // Set up the basic page framework.
  SetUpResources();
}

AddSupervisionUI::~AddSupervisionUI() = default;

bool AddSupervisionUI::CloseDialog() {
  bool showing_confirm_dialog = MaybeShowConfirmSignoutDialog();
  if (!showing_confirm_dialog) {
    // We aren't showing the confirm dialog, so close the AddSupervisionDialog.
    AddSupervisionDialog::Close();
  }
  return !showing_confirm_dialog;
}

// static
void AddSupervisionUI::SetUpForTest(signin::IdentityManager* identity_manager) {
  test_identity_manager_ = identity_manager;
}

void AddSupervisionUI::BindAddSupervisionHandler(
    mojo::PendingReceiver<add_supervision::mojom::AddSupervisionHandler>
        receiver) {
  signin::IdentityManager* identity_manager =
      test_identity_manager_
          ? test_identity_manager_
          : IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()));

  mojo_api_handler_ = std::make_unique<AddSupervisionHandler>(
      std::move(receiver), web_ui(), identity_manager, this);
}

void AddSupervisionUI::SetUpResources() {
  Profile* profile = Profile::FromWebUI(web_ui());
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIAddSupervisionHost));

  // Initialize supervision URL from the command-line arguments (if provided).
  supervision_url_ = GetAddSupervisionURL();
  if (!allow_non_google_url_for_tests_) {
    DCHECK(supervision_url_.DomainIs("google.com"));
  }

  // Forward data to the WebUI.
  source->AddResourcePath("post_message_api.js",
                          IDR_ADD_SUPERVISION_POST_MESSAGE_API_JS);
  source->AddResourcePath("add_supervision_api_server.js",
                          IDR_ADD_SUPERVISION_API_SERVER_JS);
  source->AddResourcePath("add_supervision.js", IDR_ADD_SUPERVISION_JS);
  source->AddResourcePath("images/network_unavailable.svg",
                          IDR_ADD_SUPERVISION_NETWORK_UNAVAILABLE_SVG);

  source->AddLocalizedString("pageTitle", IDS_ADD_SUPERVISION_PAGE_TITLE);
  source->AddLocalizedString("networkDownHeading",
                             IDS_ADD_SUPERVISION_NETWORK_DOWN_HEADING);
  source->AddLocalizedString("networkDownDescription",
                             IDS_ADD_SUPERVISION_NETWORK_DOWN_DESCRIPTION);
  source->AddLocalizedString("networkDownButtonLabel",
                             IDS_ADD_SUPERVISION_NETWORK_DOWN_BUTTON_LABEL);

  // Full paths (relative to src) are important for Mojom generated files.
  source->AddResourcePath(
      "chrome/browser/ui/webui/chromeos/add_supervision/"
      "add_supervision.mojom-lite.js",
      IDR_ADD_SUPERVISION_MOJOM_LITE_JS);

  source->UseStringsJs();
  source->SetDefaultResource(IDR_ADD_SUPERVISION_HTML);
  source->AddString("webviewUrl", supervision_url_.spec());
  source->AddString("eventOriginFilter", supervision_url_.GetOrigin().spec());
  source->AddString("platformVersion", base::SysInfo::OperatingSystemVersion());
  source->AddString("flowType", kAddSupervisionFlowType);

  // Forward the browser language code.
  source->AddString(
      "languageCode",
      google_util::GetGoogleLocale(g_browser_process->GetApplicationLocale()));

  content::WebUIDataSource::Add(profile, source.release());
}

// Returns the URL of the Add Supervision flow from the command-line switch,
// or the default value if it's not defined.
GURL AddSupervisionUI::GetAddSupervisionURL() {
  std::string url;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kAddSupervisionSwitch)) {
    url = command_line->GetSwitchValueASCII(kAddSupervisionSwitch);
    // The URL should only be set on the command line for testing purposes,
    // which may include pointing to a non-google URL (i.e. http://localhost/).
    // Therefore, we allow non-Google URLs in this instance.
    allow_non_google_url_for_tests_ = true;
  } else {
    url = kAddSupervisionDefaultURL;
  }
  const GURL result(url);
  DCHECK(result.is_valid()) << "Invalid URL \"" << url << "\" for switch \""
                            << kAddSupervisionSwitch << "\"";
  return result;
}

}  // namespace chromeos
