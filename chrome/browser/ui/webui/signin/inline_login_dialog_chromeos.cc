// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos.h"

#include <algorithm>
#include <string>

#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "base/check_op.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/url_util.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

InlineLoginDialogChromeOS* dialog = nullptr;
constexpr int kSigninDialogWidth = 768;
constexpr int kSigninDialogHeight = 640;

constexpr char kAccountAdditionSource[] =
    "AccountManager.AccountAdditionSource";

// Keep in sync with resources/chromeos/account_manager_error.js
enum class AccountManagerErrorType {
  kSecondaryAccountsDisabled = 0,
  kChildUserArcDisabled = 1
};

bool IsDeviceAccountEmail(const std::string& email) {
  auto* active_user = user_manager::UserManager::Get()->GetActiveUser();
  return active_user &&
         gaia::AreEmailsSame(active_user->GetDisplayEmail(), email);
}

GURL GetUrlWithEmailParam(base::StringPiece url_string,
                          const std::string& email) {
  GURL url = GURL(url_string);
  if (!email.empty()) {
    url = net::AppendQueryParameter(url, "email", email);
    url = net::AppendQueryParameter(url, "readOnlyEmail", "true");
  }
  return url;
}

GURL GetInlineLoginUrl(const std::string& email,
                       const InlineLoginDialogChromeOS::Source& source) {
  if (IsDeviceAccountEmail(email)) {
    // It's a device account re-auth.
    return GetUrlWithEmailParam(chrome::kChromeUIChromeSigninURL, email);
  }
  if (!ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          chromeos::prefs::kSecondaryGoogleAccountSigninAllowed)) {
    // Addition of secondary Google Accounts is not allowed.
    return GURL(chrome::kChromeUIAccountManagerErrorURL);
  }

  // Addition of secondary Google Accounts is allowed.
  if (!ProfileManager::GetActiveUserProfile()->IsChild()) {
    return GetUrlWithEmailParam(chrome::kChromeUIChromeSigninURL, email);
  }
  // User type is Child.
  if (!arc::IsSecondaryAccountForChildEnabled() &&
      source == InlineLoginDialogChromeOS::Source::kArc) {
    return GURL(chrome::kChromeUIAccountManagerErrorURL);
  }
  return GetUrlWithEmailParam(
      SupervisedUserService::GetEduCoexistenceLoginUrl(), email);
}

}  // namespace

// static
void InlineLoginDialogChromeOS::Show(const std::string& email,
                                     const Source& source) {
  base::UmaHistogramEnumeration(kAccountAdditionSource, source);
  // If the dialog was triggered as a response to background request, it could
  // get displayed on the lock screen. In this case it is safe to ignore it,
  // since in this case user will get it again after a request to Google
  // properties.
  if (session_manager::SessionManager::Get()->IsUserSessionBlocked())
    return;

  if (dialog) {
    dialog->dialog_window()->Focus();
    return;
  }

  // Will be deleted by |SystemWebDialogDelegate::OnDialogClosed|.
  dialog =
      new InlineLoginDialogChromeOS(GetInlineLoginUrl(email, source), source);
  dialog->ShowSystemDialog();

  // TODO(crbug.com/1016828): Remove/update this after the dialog behavior on
  // Chrome OS is defined.
  ash::WindowBackdrop::Get(dialog->dialog_window())
      ->SetBackdropType(ash::WindowBackdrop::BackdropType::kSemiOpaque);
}

void InlineLoginDialogChromeOS::Show(const Source& source) {
  Show(/* email= */ std::string(), source);
}

// static
void InlineLoginDialogChromeOS::UpdateEduCoexistenceFlowResult(
    EduCoexistenceFlowResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (dialog)
    dialog->SetEduCoexistenceFlowResult(result);
}

void InlineLoginDialogChromeOS::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->z_order = ui::ZOrderLevel::kNormal;
}

gfx::Size InlineLoginDialogChromeOS::GetMaximumDialogSize() {
  gfx::Size size;
  GetDialogSize(&size);
  return size;
}

gfx::NativeView InlineLoginDialogChromeOS::GetHostView() const {
  return dialog_window();
}

gfx::Point InlineLoginDialogChromeOS::GetDialogPosition(const gfx::Size& size) {
  gfx::Size host_size = GetHostView()->bounds().size();

  // Show all sub-dialogs at center-top.
  return gfx::Point(std::max(0, (host_size.width() - size.width()) / 2), 0);
}

void InlineLoginDialogChromeOS::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {}

void InlineLoginDialogChromeOS::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {}

void InlineLoginDialogChromeOS::SetEduCoexistenceFlowResult(
    EduCoexistenceFlowResult result) {
  edu_coexistence_flow_result_ = result;
}

InlineLoginDialogChromeOS::InlineLoginDialogChromeOS(const GURL& url,
                                                     const Source& source)
    : SystemWebDialogDelegate(url, base::string16() /* title */),
      delegate_(this),
      source_(source),
      url_(url) {}

InlineLoginDialogChromeOS::~InlineLoginDialogChromeOS() {
  DCHECK_EQ(this, dialog);
  dialog = nullptr;
}

void InlineLoginDialogChromeOS::GetDialogSize(gfx::Size* size) const {
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(dialog_window());
  size->SetSize(std::min(kSigninDialogWidth, display.work_area().width()),
                std::min(kSigninDialogHeight, display.work_area().height()));
}

std::string InlineLoginDialogChromeOS::GetDialogArgs() const {
  if (url_.GetWithEmptyPath() !=
      GURL(chrome::kChromeUIAccountManagerErrorURL)) {
    return std::string();
  }

  AccountManagerErrorType error =
      AccountManagerErrorType::kSecondaryAccountsDisabled;
  if (source_ == Source::kArc &&
      ProfileManager::GetActiveUserProfile()->IsChild() &&
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          chromeos::prefs::kSecondaryGoogleAccountSigninAllowed)) {
    error = AccountManagerErrorType::kChildUserArcDisabled;
  }

  std::string data;
  base::DictionaryValue dialog_args;
  dialog_args.SetInteger("errorType", static_cast<int>(error));
  base::JSONWriter::Write(dialog_args, &data);
  return data;
}

bool InlineLoginDialogChromeOS::ShouldShowDialogTitle() const {
  return false;
}

void InlineLoginDialogChromeOS::OnDialogShown(content::WebUI* webui) {
  SystemWebDialogDelegate::OnDialogShown(webui);
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      webui->GetWebContents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      webui->GetWebContents())
      ->SetDelegate(&delegate_);
}

void InlineLoginDialogChromeOS::OnDialogClosed(const std::string& json_retval) {
  if (ProfileManager::GetActiveUserProfile()->IsChild()) {
    DCHECK(edu_coexistence_flow_result_.has_value());
    base::UmaHistogramEnumeration("AccountManager.EduCoexistence.FlowResult",
                                  edu_coexistence_flow_result_.value());
  }
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

}  // namespace chromeos
