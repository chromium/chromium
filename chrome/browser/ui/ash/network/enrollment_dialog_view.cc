// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/enrollment_dialog_view.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash::enrollment {

namespace {

// Default width/height of the dialog.
const int kDefaultWidth = 350;
const int kDefaultHeight = 100;

////////////////////////////////////////////////////////////////////////////////
// Dialog for certificate enrollment. This displays the content from the
// certificate enrollment URI.
class EnrollmentDialogView : public views::DialogDelegateView {
 public:
  ~EnrollmentDialogView() override;

  static void ShowDialog(const std::string& network_name,
                         Profile* profile,
                         const GURL& target_uri);

  // views::DialogDelegateView overrides
  bool Accept() override;

  // views::WidgetDelegate overrides
  ui::ModalType GetModalType() const override;
  void WindowClosing() override;

  // views::View overrides
  gfx::Size CalculatePreferredSize() const override;

 private:
  EnrollmentDialogView(const std::string& network_name,
                       Profile* profile,
                       const GURL& target_uri);
  void InitDialog();

  bool accepted_;
  std::string network_name_;
  raw_ptr<Profile, ExperimentalAsh> profile_;
  GURL target_uri_;
};

////////////////////////////////////////////////////////////////////////////////
// EnrollmentDialogView implementation.

EnrollmentDialogView::EnrollmentDialogView(const std::string& network_name,
                                           Profile* profile,
                                           const GURL& target_uri)
    : accepted_(false),
      network_name_(network_name),
      profile_(profile),
      target_uri_(target_uri) {
  SetTitle(l10n_util::GetStringUTF16(IDS_NETWORK_ENROLLMENT_HANDLER_TITLE));
  DialogDelegate::SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_NETWORK_ENROLLMENT_HANDLER_BUTTON));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));

  SetUseDefaultFillLayout(true);
  auto* label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(IDS_NETWORK_ENROLLMENT_HANDLER_INSTRUCTIONS,
                                 base::UTF8ToUTF16(network_name_))));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  label->SetAllowCharacterBreak(true);
}

EnrollmentDialogView::~EnrollmentDialogView() = default;

// static
void EnrollmentDialogView::ShowDialog(const std::string& network_name,
                                      Profile* profile,
                                      const GURL& target_uri) {
  EnrollmentDialogView* dialog_view =
      new EnrollmentDialogView(network_name, profile, target_uri);

  views::Widget::InitParams params =
      views::DialogDelegate::GetDialogWidgetInitParams(
          dialog_view, nullptr /* context */, nullptr /* parent */,
          gfx::Rect() /* bounds */);
  ash_util::SetupWidgetInitParamsForContainer(
      &params, ash_util::GetSystemModalDialogContainerId());
  views::Widget* widget = new views::Widget;  // Owned by native widget.
  widget->Init(std::move(params));
  widget->Show();
}

bool EnrollmentDialogView::Accept() {
  accepted_ = true;
  return true;
}

ui::ModalType EnrollmentDialogView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

void EnrollmentDialogView::WindowClosing() {
  if (!accepted_)
    return;
  NavigateParams params(profile_, GURL(target_uri_), ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);
}

gfx::Size EnrollmentDialogView::CalculatePreferredSize() const {
  return gfx::Size(kDefaultWidth, kDefaultHeight);
}

////////////////////////////////////////////////////////////////////////////////
// Handler for certificate enrollment.

class DialogEnrollmentDelegate {
 public:
  // |owning_window| is the window that will own the dialog.
  DialogEnrollmentDelegate(const std::string& network_guid,
                           const std::string& network_name,
                           Profile* profile);

  DialogEnrollmentDelegate(const DialogEnrollmentDelegate&) = delete;
  DialogEnrollmentDelegate& operator=(const DialogEnrollmentDelegate&) = delete;

  ~DialogEnrollmentDelegate();

  bool Enroll(const std::vector<std::string>& uri_list);

 private:
  std::string network_guid_;
  std::string network_name_;
  raw_ptr<Profile, ExperimentalAsh> profile_;
};

DialogEnrollmentDelegate::DialogEnrollmentDelegate(
    const std::string& network_guid,
    const std::string& network_name,
    Profile* profile)
    : network_guid_(network_guid),
      network_name_(network_name),
      profile_(profile) {}

DialogEnrollmentDelegate::~DialogEnrollmentDelegate() = default;

bool DialogEnrollmentDelegate::Enroll(
    const std::vector<std::string>& uri_list) {
  // Keep the closure for later activation if we notice that
  // a certificate has been added.

  // TODO(gspencer): Do something smart with the closure.  At the moment it is
  // being ignored because we don't know when the enrollment tab is closed.
  // http://crosbug.com/30422
  for (std::vector<std::string>::const_iterator iter = uri_list.begin();
       iter != uri_list.end(); ++iter) {
    GURL uri(*iter);
    if (uri.IsStandard() || uri.scheme() == extensions::kExtensionScheme) {
      // If this is a "standard" scheme, like http, ftp, etc., then open that in
      // the enrollment dialog.
      NET_LOG(EVENT) << "Showing enrollment dialog for: "
                     << NetworkGuidId(network_guid_);
      EnrollmentDialogView::ShowDialog(network_name_, profile_, uri);
      return true;
    }
    NET_LOG(DEBUG) << "Nonstandard URI: " + uri.spec()
                   << " For: " << NetworkGuidId(network_guid_);
  }

  // No appropriate scheme was found.
  NET_LOG(ERROR) << "No usable enrollment URI for: "
                 << NetworkGuidId(network_guid_);
  return false;
}

// Decides if the enrollment dialog is allowed in the current login state.
bool EnrollmentDialogAllowed(Profile* profile) {
  // Enrollment dialog is currently not supported on the sign-in profile.
  // This also applies to lock screen,
  if (ProfileHelper::IsSigninProfile(profile))
    return false;

  LoginState::LoggedInUserType user_type =
      LoginState::Get()->GetLoggedInUserType();
  switch (user_type) {
    case LoginState::LOGGED_IN_USER_NONE:
      return false;
    case LoginState::LOGGED_IN_USER_REGULAR:
      return true;
    case LoginState::LOGGED_IN_USER_OWNER:
      return true;
    case LoginState::LOGGED_IN_USER_GUEST:
      return true;
    case LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT:
      return false;
    case LoginState::LOGGED_IN_USER_KIOSK:
      return false;
    case LoginState::LOGGED_IN_USER_CHILD:
      return true;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Factory function.

bool CreateEnrollmentDialog(const std::string& network_id) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          network_id);
  if (!network) {
    NET_LOG(ERROR) << "Enrolling Unknown network: "
                   << NetworkGuidId(network_id);
    return false;
  }
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!EnrollmentDialogAllowed(profile)) {
    return false;
  }
  std::string username_hash = ProfileHelper::GetUserIdHashFromProfile(profile);

  onc::ONCSource onc_source = onc::ONC_SOURCE_NONE;
  const base::Value::Dict* policy =
      NetworkHandler::Get()
          ->managed_network_configuration_handler()
          ->FindPolicyByGUID(username_hash, network_id, &onc_source);

  if (!policy) {
    return false;
  }

  client_cert::ClientCertConfig cert_config;
  OncToClientCertConfig(onc_source, *policy, &cert_config);

  if (cert_config.client_cert_type != onc::client_cert::kPattern) {
    return false;
  }

  if (cert_config.pattern.Empty()) {
    NET_LOG(ERROR) << "Certificate pattern is empty for: "
                   << NetworkGuidId(network_id);
  }

  if (cert_config.pattern.enrollment_uri_list().empty()) {
    NET_LOG(EVENT) << "No enrollment URIs for: " << NetworkGuidId(network_id);
    return false;
  }

  NET_LOG(USER) << "Enrolling: " << NetworkGuidId(network_id);

  DialogEnrollmentDelegate* enrollment =
      new DialogEnrollmentDelegate(network_id, network->name(), profile);
  return enrollment->Enroll(cert_config.pattern.enrollment_uri_list());
}

}  // namespace ash::enrollment
