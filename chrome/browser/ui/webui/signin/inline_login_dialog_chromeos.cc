// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos.h"

#include <algorithm>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_features.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
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

// The EDU Coexistence signin dialog uses different dimensions
// that match the dimensions of the equivalent OOBE
// dialog, and are required for the size of the web content
// that the dialog hosts.
constexpr int kEduCoexistenceSigninDialogWidth = 1040;
constexpr int kEduCoexistenceSigninDialogHeight = 680;

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

GURL GetInlineLoginUrl(const std::string& email) {
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
  if (ProfileManager::GetActiveUserProfile()->IsChild()) {
    return GetUrlWithEmailParam(
        SupervisedUserService::GetEduCoexistenceLoginUrl(), email);
  }
  return GetUrlWithEmailParam(chrome::kChromeUIChromeSigninURL, email);
}

}  // namespace

// static
bool InlineLoginDialogChromeOS::IsShown() {
  return dialog != nullptr;
}

// static
void InlineLoginDialogChromeOS::ShowDeprecated(
    const std::string& email,
    const ::account_manager::AccountManagerFacade::AccountAdditionSource&
        source) {
  base::UmaHistogramEnumeration(
      account_manager::AccountManagerFacade::kAccountAdditionSource, source);
  ShowInternal(email);
}

// static
void InlineLoginDialogChromeOS::ShowDeprecated(
    const ::account_manager::AccountManagerFacade::AccountAdditionSource&
        source) {
  ShowDeprecated(/* email= */ std::string(), source);
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
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observer_list_.AddObserver(observer);
}

void InlineLoginDialogChromeOS::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observer_list_.RemoveObserver(observer);
}

InlineLoginDialogChromeOS::InlineLoginDialogChromeOS()
    : InlineLoginDialogChromeOS(GetInlineLoginUrl(std::string())) {}

InlineLoginDialogChromeOS::InlineLoginDialogChromeOS(const GURL& url)
    : SystemWebDialogDelegate(url, std::u16string() /* title */),
      delegate_(this),
      url_(url) {
  DCHECK(!dialog);
  dialog = this;
}

InlineLoginDialogChromeOS::InlineLoginDialogChromeOS(
    const GURL& url,
    base::OnceClosure close_dialog_closure)
    : SystemWebDialogDelegate(url, std::u16string() /* title */),
      delegate_(this),
      url_(url),
      close_dialog_closure_(std::move(close_dialog_closure)) {
  DCHECK(!dialog);
  dialog = this;
}

InlineLoginDialogChromeOS::~InlineLoginDialogChromeOS() {
  for (auto& observer : modal_dialog_host_observer_list_)
    observer.OnHostDestroying();

  if (webui()) {
    web_modal::WebContentsModalDialogManager::FromWebContents(
        webui()->GetWebContents())
        ->SetDelegate(nullptr);
  }

  if (!close_dialog_closure_.is_null()) {
    std::move(close_dialog_closure_).Run();
  }

  DCHECK_EQ(this, dialog);
  dialog = nullptr;
}

void InlineLoginDialogChromeOS::GetDialogSize(gfx::Size* size) const {
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(dialog_window());

  if (ProfileManager::GetActiveUserProfile()->IsChild() &&
      base::FeatureList::IsEnabled(supervised_users::kEduCoexistenceFlowV2)) {
    size->SetSize(
        std::min(kEduCoexistenceSigninDialogWidth, display.work_area().width()),
        std::min(kEduCoexistenceSigninDialogHeight,
                 display.work_area().height()));
    return;
  }

  size->SetSize(std::min(kSigninDialogWidth, display.work_area().width()),
                std::min(kSigninDialogHeight, display.work_area().height()));
}

ui::ModalType InlineLoginDialogChromeOS::GetDialogModalType() const {
  return chromeos::features::IsAccountManagementFlowsV2Enabled()
             ? ui::MODAL_TYPE_SYSTEM
             : ui::MODAL_TYPE_NONE;
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
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

// static
void InlineLoginDialogChromeOS::Show(base::OnceClosure close_dialog_closure) {
  Show(/* email= */ std::string(), std::move(close_dialog_closure));
}

// static
void InlineLoginDialogChromeOS::Show(const std::string& email,
                                     base::OnceClosure close_dialog_closure) {
  ShowInternal(email, std::move(close_dialog_closure));
}

// static
void InlineLoginDialogChromeOS::ShowInternal(
    const std::string& email,
    base::OnceClosure close_dialog_closure) {
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
  dialog = new InlineLoginDialogChromeOS(GetInlineLoginUrl(email),
                                         std::move(close_dialog_closure));
  dialog->ShowSystemDialog();

  // TODO(crbug.com/1016828): Remove/update this after the dialog behavior on
  // Chrome OS is defined.
  ash::WindowBackdrop::Get(dialog->dialog_window())
      ->SetBackdropType(ash::WindowBackdrop::BackdropType::kSemiOpaque);
}

}  // namespace chromeos
