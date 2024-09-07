// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/ash/inline_login_dialog.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "ash/public/cpp/window_backdrop.h"
#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "components/account_manager_core/account_addition_options.h"
#include "components/account_manager_core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/url_util.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {

InlineLoginDialog* dialog = nullptr;
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

GURL GetUrlWithEmailParam(std::string_view url_string,
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
          ::account_manager::prefs::kSecondaryGoogleAccountSigninAllowed)) {
    // Addition of secondary Google Accounts is not allowed.
    return GURL(chrome::kChromeUIAccountManagerErrorURL);
  }

  // Addition of secondary Google Accounts is allowed.
  if (ProfileManager::GetActiveUserProfile()->IsChild()) {
    return GetUrlWithEmailParam(chrome::kChromeUIEDUCoexistenceLoginURLV2,
                                email);
  }
  return GetUrlWithEmailParam(chrome::kChromeUIChromeSigninURL, email);
}

// Convert `options` to `base::Value`. Keep fields in sync with
// chrome/browser/resources/inline_login/inline_login_app.js
base::Value AccountAdditionOptionsToValue(
    const account_manager::AccountAdditionOptions& options) {
  base::Value::Dict args;
  args.Set("isAvailableInArc", base::Value(options.is_available_in_arc));
  args.Set("showArcAvailabilityPicker",
           base::Value(options.show_arc_availability_picker));
  return base::Value(std::move(args));
}

}  // namespace

// Cleans up the delegate for a WebContentsModalDialogManager on destruction, or
// on WebContents destruction, whichever comes first.
class InlineLoginDialog::ModalDialogManagerCleanup
    : public content::WebContentsObserver {
 public:
  // This constructor automatically observes |web_contents| for its lifetime.
  explicit ModalDialogManagerCleanup(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ModalDialogManagerCleanup(const ModalDialogManagerCleanup&) = delete;
  ModalDialogManagerCleanup& operator=(const ModalDialogManagerCleanup&) =
      delete;
  ~ModalDialogManagerCleanup() override { ResetDelegate(); }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override { ResetDelegate(); }

  void ResetDelegate() {
    if (!web_contents())
      return;
    web_modal::WebContentsModalDialogManager::FromWebContents(web_contents())
        ->SetDelegate(nullptr);
  }
};

// static
bool InlineLoginDialog::IsShown() {
  return dialog != nullptr;
}

void InlineLoginDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->z_order = ui::ZOrderLevel::kNormal;
}

gfx::Size InlineLoginDialog::GetMaximumDialogSize() {
  gfx::Size size;
  GetDialogSize(&size);
  return size;
}

gfx::NativeView InlineLoginDialog::GetHostView() const {
  return dialog_window();
}

gfx::Point InlineLoginDialog::GetDialogPosition(const gfx::Size& size) {
  gfx::Size host_size = GetHostView()->bounds().size();

  // Show all sub-dialogs at center-top.
  return gfx::Point(std::max(0, (host_size.width() - size.width()) / 2), 0);
}

void InlineLoginDialog::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observer_list_.AddObserver(observer);
}

void InlineLoginDialog::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observer_list_.RemoveObserver(observer);
}

InlineLoginDialog::InlineLoginDialog()
    : InlineLoginDialog(GetInlineLoginUrl(std::string())) {}

InlineLoginDialog::InlineLoginDialog(const GURL& url)
    : InlineLoginDialog(url, std::nullopt, base::DoNothing()) {}

InlineLoginDialog::InlineLoginDialog(
    const GURL& url,
    std::optional<account_manager::AccountAdditionOptions> options,
    base::OnceClosure close_dialog_closure)
    : SystemWebDialogDelegate(url, std::u16string() /* title */),
      delegate_(this),
      url_(url),
      add_account_options_(options),
      close_dialog_closure_(std::move(close_dialog_closure)) {
  DCHECK(!dialog);
  dialog = this;
}

InlineLoginDialog::~InlineLoginDialog() {
  for (auto& observer : modal_dialog_host_observer_list_)
    observer.OnHostDestroying();

  if (!close_dialog_closure_.is_null()) {
    std::move(close_dialog_closure_).Run();
  }

  DCHECK_EQ(this, dialog);
  dialog = nullptr;
}

void InlineLoginDialog::GetDialogSize(gfx::Size* size) const {
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(dialog_window());

  if (ProfileManager::GetActiveUserProfile()->IsChild()) {
    size->SetSize(
        std::min(kEduCoexistenceSigninDialogWidth, display.work_area().width()),
        std::min(kEduCoexistenceSigninDialogHeight,
                 display.work_area().height()));
    return;
  }

  size->SetSize(std::min(kSigninDialogWidth, display.work_area().width()),
                std::min(kSigninDialogHeight, display.work_area().height()));
}

ui::mojom::ModalType InlineLoginDialog::GetDialogModalType() const {
  return ui::mojom::ModalType::kSystem;
}

bool InlineLoginDialog::ShouldShowDialogTitle() const {
  return false;
}

void InlineLoginDialog::OnDialogShown(content::WebUI* webui) {
  SystemWebDialogDelegate::OnDialogShown(webui);
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      webui->GetWebContents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      webui->GetWebContents())
      ->SetDelegate(&delegate_);
  modal_dialog_manager_cleanup_ =
      std::make_unique<ModalDialogManagerCleanup>(webui->GetWebContents());
}

void InlineLoginDialog::OnDialogClosed(const std::string& json_retval) {
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

// The args value will be available from JS via
// chrome.getVariableValue('dialogArguments').
std::string InlineLoginDialog::GetDialogArgs() const {
  if (!add_account_options_)
    return std::string();

  std::string json;
  base::JSONWriter::Write(
      AccountAdditionOptionsToValue(add_account_options_.value()), &json);
  return json;
}

// static
void InlineLoginDialog::Show(
    const account_manager::AccountAdditionOptions& options,
    base::OnceClosure close_dialog_closure) {
  ShowInternal(/* email= */ std::string(), options,
               std::move(close_dialog_closure));
}

// static
void InlineLoginDialog::Show(const std::string& email,
                             base::OnceClosure close_dialog_closure) {
  ShowInternal(email, /*options=*/std::nullopt,
               std::move(close_dialog_closure));
}

// static
void InlineLoginDialog::ShowInternal(
    const std::string& email,
    std::optional<account_manager::AccountAdditionOptions> options,
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
  dialog = new InlineLoginDialog(GetInlineLoginUrl(email), options,
                                 std::move(close_dialog_closure));
  dialog->ShowSystemDialog();

  // TODO(crbug.com/1016828): Remove/update this after the dialog behavior on
  // Chrome OS is defined.
  WindowBackdrop::Get(dialog->dialog_window())
      ->SetBackdropType(WindowBackdrop::BackdropType::kSemiOpaque);
}

}  // namespace ash
