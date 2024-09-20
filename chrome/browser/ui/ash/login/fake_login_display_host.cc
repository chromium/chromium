// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/fake_login_display_host.h"

#include "base/notreached.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {

class FakeLoginDisplayHost::FakeBaseScreen : public BaseScreen {
 public:
  explicit FakeBaseScreen(OobeScreenId screen_id)
      : BaseScreen(screen_id, OobeScreenPriority::DEFAULT) {}

  FakeBaseScreen(const FakeBaseScreen&) = delete;
  FakeBaseScreen& operator=(const FakeBaseScreen&) = delete;

  ~FakeBaseScreen() override = default;

 private:
  // BaseScreen:
  void ShowImpl() override {}
  void HideImpl() override {}
};

FakeLoginDisplayHost::FakeLoginDisplayHost()
    : wizard_context_(std::make_unique<WizardContext>()),
      oobe_metrics_helper_(std::make_unique<OobeMetricsHelper>()) {
  // Only one SessionManager can be instantiated at a time. Check to see if one
  // has already been instantiated before creating one.
  if (!session_manager::SessionManager::Get()) {
    session_manager_ = std::make_unique<session_manager::SessionManager>();
  }
}

FakeLoginDisplayHost::~FakeLoginDisplayHost() = default;

ExistingUserController* FakeLoginDisplayHost::GetExistingUserController() {
  return nullptr;
}

gfx::NativeWindow FakeLoginDisplayHost::GetNativeWindow() const {
  return nullptr;
}

views::Widget* FakeLoginDisplayHost::GetLoginWindowWidget() const {
  return nullptr;
}

OobeUI* FakeLoginDisplayHost::GetOobeUI() const {
  return oobe_ui_;
}

content::WebContents* FakeLoginDisplayHost::GetOobeWebContents() const {
  return nullptr;
}

WebUILoginView* FakeLoginDisplayHost::GetWebUILoginView() const {
  return nullptr;
}

void FakeLoginDisplayHost::BeforeSessionStart() {}

bool FakeLoginDisplayHost::IsFinalizing() {
  return false;
}

void FakeLoginDisplayHost::FinalizeImmediately() {}

void FakeLoginDisplayHost::StartWizard(OobeScreenId first_screen) {
  if (wizard_controller_) {
    wizard_controller_->AdvanceToScreen(first_screen);
  } else {
    wizard_controller_ =
        std::make_unique<WizardController>(wizard_context_.get());

    fake_screen_ = std::make_unique<FakeBaseScreen>(first_screen);
    wizard_controller_->SetCurrentScreenForTesting(fake_screen_.get());
  }
}

WizardController* FakeLoginDisplayHost::GetWizardController() {
  return wizard_controller_.get();
}

WizardContext* FakeLoginDisplayHost::GetWizardContext() {
  return wizard_context_.get();
}

void FakeLoginDisplayHost::CancelUserAdding() {}

void FakeLoginDisplayHost::StartSignInScreen() {
  StartWizard(UserCreationView::kScreenId);
}

void FakeLoginDisplayHost::StartKiosk(const KioskAppId& kiosk_app_id,
                                      bool is_auto_launch) {}

void FakeLoginDisplayHost::CompleteLogin(const UserContext& user_context) {}

void FakeLoginDisplayHost::OnGaiaScreenReady() {}

void FakeLoginDisplayHost::SetDisplayEmail(const std::string& email) {}

void FakeLoginDisplayHost::UpdateWallpaper(const AccountId& prefilled_account) {
}

bool FakeLoginDisplayHost::IsUserAllowlisted(
    const AccountId& account_id,
    const std::optional<user_manager::UserType>& user_type) {
  return false;
}

void FakeLoginDisplayHost::ShowGaiaDialog(const AccountId& prefilled_account) {}

void FakeLoginDisplayHost::StartUserRecovery(
    const AccountId& account_to_recover) {}

void FakeLoginDisplayHost::ShowAllowlistCheckFailedError() {}

void FakeLoginDisplayHost::ShowOsInstallScreen() {}

void FakeLoginDisplayHost::ShowGuestTosScreen() {}

void FakeLoginDisplayHost::ShowRemoteActivityNotificationScreen() {}

void FakeLoginDisplayHost::HideOobeDialog(bool saml_page_closed) {}

void FakeLoginDisplayHost::SetShelfButtonsEnabled(bool enabled) {}

void FakeLoginDisplayHost::UpdateOobeDialogState(OobeDialogState state) {}

void FakeLoginDisplayHost::CancelPasswordChangedFlow() {}

bool FakeLoginDisplayHost::HandleAccelerator(LoginAcceleratorAction action) {
  return false;
}

void FakeLoginDisplayHost::HandleDisplayCaptivePortal() {}

void FakeLoginDisplayHost::UpdateAddUserButtonStatus() {}

void FakeLoginDisplayHost::RequestSystemInfoUpdate() {}

bool FakeLoginDisplayHost::HasUserPods() {
  return false;
}

void FakeLoginDisplayHost::AddObserver(LoginDisplayHost::Observer* observer) {}

void FakeLoginDisplayHost::RemoveObserver(
    LoginDisplayHost::Observer* observer) {}

SigninUI* FakeLoginDisplayHost::GetSigninUI() {
  return nullptr;
}

void FakeLoginDisplayHost::AddWizardCreatedObserverForTests(
    base::RepeatingClosure on_created) {
  NOTREACHED_IN_MIGRATION();
}

bool FakeLoginDisplayHost::IsWizardControllerCreated() const {
  return wizard_controller_.get();
}

WizardContext* FakeLoginDisplayHost::GetWizardContextForTesting() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool FakeLoginDisplayHost::IsWebUIStarted() const {
  return wizard_controller_.get();
}

base::WeakPtr<ash::quick_start::TargetDeviceBootstrapController>
FakeLoginDisplayHost::GetQuickStartBootstrapController() {
  return nullptr;
}

bool FakeLoginDisplayHost::GetKeyboardRemappedPrefValue(
    const std::string& pref_name,
    int* value) const {
  return false;
}

void FakeLoginDisplayHost::SetOobeUI(OobeUI* oobe_ui) {
  oobe_ui_ = oobe_ui;
}

void FakeLoginDisplayHost::SetWizardController(
    std::unique_ptr<WizardController> wizard_controller) {
  wizard_controller_ = std::move(wizard_controller);
}

OobeMetricsHelper* FakeLoginDisplayHost::GetOobeMetricsHelper() {
  return oobe_metrics_helper_.get();
}

}  // namespace ash
