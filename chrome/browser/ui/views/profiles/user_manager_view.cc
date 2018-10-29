// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/user_manager_view.h"

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

#if defined(OS_WIN)
#include "chrome/browser/shell_integration_win.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/app_controller_mac.h"
#endif

namespace {

// An open User Manager window. There can only be one open at a time. This
// is reset to NULL when the window is closed.
UserManagerView* g_user_manager_view = nullptr;
base::Closure* g_user_manager_shown_callback_for_testing = nullptr;
bool g_is_user_manager_view_under_construction = false;
}  // namespace

// Delegate---------------------------------------------------------------

UserManagerProfileDialogDelegate::UserManagerProfileDialogDelegate(
    UserManagerView* parent,
    views::WebView* web_view,
    const std::string& email_address,
    const GURL& url)
    : parent_(parent), web_view_(web_view), email_address_(email_address) {
  AddChildView(web_view_);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  web_view_->GetWebContents()->SetDelegate(this);
  web_view_->LoadInitialURL(url);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::USER_MANAGER_PROFILE);
}

UserManagerProfileDialogDelegate::~UserManagerProfileDialogDelegate() {}

gfx::Size UserManagerProfileDialogDelegate::CalculatePreferredSize() const {
  return gfx::Size(UserManagerProfileDialog::kDialogWidth,
                   UserManagerProfileDialog::kDialogHeight);
}

void UserManagerProfileDialogDelegate::DisplayErrorMessage() {
  web_view_->LoadInitialURL(GURL(chrome::kChromeUISigninErrorURL));
}

bool UserManagerProfileDialogDelegate::CanResize() const {
  return true;
}

bool UserManagerProfileDialogDelegate::CanMaximize() const {
  return true;
}

bool UserManagerProfileDialogDelegate::CanMinimize() const {
  return true;
}

bool UserManagerProfileDialogDelegate::ShouldUseCustomFrame() const {
  return false;
}

ui::ModalType UserManagerProfileDialogDelegate::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void UserManagerProfileDialogDelegate::DeleteDelegate() {
  OnDialogDestroyed();
  delete this;
}

base::string16 UserManagerProfileDialogDelegate::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_PROFILES_GAIA_SIGNIN_TITLE);
}

int UserManagerProfileDialogDelegate::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

views::View* UserManagerProfileDialogDelegate::GetInitiallyFocusedView() {
  return static_cast<views::View*>(web_view_);
}

void UserManagerProfileDialogDelegate::CloseDialog() {
  OnDialogDestroyed();
  GetWidget()->Close();
}

void UserManagerProfileDialogDelegate::OnDialogDestroyed() {
  if (parent_) {
    parent_->OnDialogDestroyed();
    parent_ = nullptr;
  }
}

// UserManager -----------------------------------------------------------------

// static
void UserManager::Show(
    const base::FilePath& profile_path_to_focus,
    profiles::UserManagerAction user_manager_action) {
  DCHECK(profile_path_to_focus != ProfileManager::GetGuestProfilePath());

  ProfileMetrics::LogProfileOpenMethod(ProfileMetrics::OPEN_USER_MANAGER);
  if (g_user_manager_view) {
    // If we are showing the User Manager after locking a profile, change the
    // active profile to Guest.
    profiles::SetActiveProfileToGuestIfLocked();

#if defined(OS_MACOSX)
    app_controller_mac::CreateGuestProfileIfNeeded();
#endif

    // Note the time we started opening the User Manager.
    g_user_manager_view->set_user_manager_started_showing(base::Time::Now());

    // If there's a user manager window open already, just activate it.
    g_user_manager_view->GetWidget()->Activate();
    return;
  }

  // Under some startup conditions, we can try twice to create the User Manager.
  // Because creating the System profile is asynchronous, it's possible for
  // there to then be multiple pending operations and eventually multiple
  // User Managers.
  if (g_is_user_manager_view_under_construction)
    return;

  // Create the system profile, if necessary, and open the user manager
  // from the system profile.
  UserManagerView* user_manager = new UserManagerView();
  user_manager->set_user_manager_started_showing(base::Time::Now());
  profiles::CreateSystemProfileForUserManager(
      profile_path_to_focus, user_manager_action,
      base::Bind(&UserManagerView::OnSystemProfileCreated,
                 base::Passed(base::WrapUnique(user_manager)),
                 base::Owned(new base::AutoReset<bool>(
                     &g_is_user_manager_view_under_construction, true))));
}

// static
void UserManager::Hide() {
  if (g_user_manager_view)
    g_user_manager_view->GetWidget()->Close();
}

// static
bool UserManager::IsShowing() {
#if defined(OS_MACOSX)
  // Widget activation works differently on Mac: the UserManager is a child
  // widget, so it is not active in the IsActive() sense even when showing
  // and interactable. Test for IsVisible instead - this is what the Cocoa
  // UserManager::IsShowing() does as well.
  return g_user_manager_view ? g_user_manager_view->GetWidget()->IsVisible()
                             : false;
#else
  return g_user_manager_view ? g_user_manager_view->GetWidget()->IsActive()
                             : false;
#endif
}

// static
void UserManager::OnUserManagerShown() {
  if (g_user_manager_view) {
    g_user_manager_view->LogTimeToOpen();
    if (g_user_manager_shown_callback_for_testing) {
      if (!g_user_manager_shown_callback_for_testing->is_null())
        g_user_manager_shown_callback_for_testing->Run();

      delete g_user_manager_shown_callback_for_testing;
      g_user_manager_shown_callback_for_testing = nullptr;
    }
  }
}

// static
void UserManager::AddOnUserManagerShownCallbackForTesting(
    const base::Closure& callback) {
  DCHECK(!g_user_manager_shown_callback_for_testing);
  g_user_manager_shown_callback_for_testing = new base::Closure(callback);
}

// static
base::FilePath UserManager::GetSigninProfilePath() {
  return g_user_manager_view->GetSigninProfilePath();
}

// UserManagerProfileDialog
// -------------------------------------------------------------

// static
void UserManagerProfileDialog::ShowReauthDialog(
    content::BrowserContext* browser_context,
    const std::string& email,
    signin_metrics::Reason reason) {
  ShowReauthDialogWithProfilePath(browser_context, email, base::FilePath(),
                                  reason);
}

// static
void UserManagerProfileDialog::ShowReauthDialogWithProfilePath(
    content::BrowserContext* browser_context,
    const std::string& email,
    const base::FilePath& profile_path,
    signin_metrics::Reason reason) {
  CHECK(signin_util::IsForceSigninEnabled() ||
        reason != signin_metrics::Reason::REASON_UNLOCK)
      << "Legacy supervised users are no longer supported.";
  // This method should only be called if the user manager is already showing.
  if (!UserManager::IsShowing())
    return;
  // Load the re-auth URL, prepopulated with the user's email address.
  // Add the index of the profile to the URL so that the inline login page
  // knows which profile to load and update the credentials.
  GURL url = signin::GetReauthURLWithEmailForDialog(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER, reason, email);
  g_user_manager_view->SetSigninProfilePath(profile_path);
  g_user_manager_view->ShowDialog(browser_context, email, url);
}

// static
void UserManagerProfileDialog::ShowSigninDialog(
    content::BrowserContext* browser_context,
    const base::FilePath& profile_path,
    signin_metrics::Reason reason) {
  if (!UserManager::IsShowing())
    return;
  DCHECK(reason ==
             signin_metrics::Reason::REASON_FORCED_SIGNIN_PRIMARY_ACCOUNT ||
         reason == signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT);
  g_user_manager_view->SetSigninProfilePath(profile_path);
  GURL url = signin::GetPromoURLForDialog(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER, reason, true);
  g_user_manager_view->ShowDialog(browser_context, std::string(), url);
}

void UserManagerProfileDialog::ShowDialogAndDisplayErrorMessage(
    content::BrowserContext* browser_context) {
  if (!UserManager::IsShowing())
    return;
  // The error occurred before sign in happened, reset |signin_profile_path_|
  // so that the error page will show the error message that is assoicated with
  // the system profile.
  g_user_manager_view->SetSigninProfilePath(base::FilePath());
  g_user_manager_view->ShowDialog(browser_context, std::string(),
                                  GURL(chrome::kChromeUISigninErrorURL));
}

// static
void UserManagerProfileDialog::DisplayErrorMessage() {
  // This method should only be called if the user manager is already showing.
  DCHECK(g_user_manager_view);
  g_user_manager_view->DisplayErrorMessage();
}

// static
void UserManagerProfileDialog::HideDialog() {
  if (g_user_manager_view && g_user_manager_view->GetWidget()->IsVisible())
    g_user_manager_view->HideDialog();
}

// UserManagerView -------------------------------------------------------------

UserManagerView::UserManagerView()
    : web_view_(nullptr),
      delegate_(nullptr),
      user_manager_started_showing_(base::Time()) {
  keep_alive_.reset(new ScopedKeepAlive(KeepAliveOrigin::USER_MANAGER_VIEW,
                                        KeepAliveRestartOption::DISABLED));
  chrome::RecordDialogCreation(chrome::DialogIdentifier::USER_MANAGER);
}

UserManagerView::~UserManagerView() {
  HideDialog();
}

// static
void UserManagerView::OnSystemProfileCreated(
    std::unique_ptr<UserManagerView> instance,
    base::AutoReset<bool>* pending,
    Profile* system_profile,
    const std::string& url) {
  // If we are showing the User Manager after locking a profile, change the
  // active profile to Guest.
  profiles::SetActiveProfileToGuestIfLocked();

#if defined(OS_MACOSX)
  app_controller_mac::CreateGuestProfileIfNeeded();
#endif

  DCHECK(!g_user_manager_view);
  g_user_manager_view =
      instance.release();  // |g_user_manager_view| takes over ownership.
  g_user_manager_view->Init(system_profile, GURL(url));
}

void UserManagerView::ShowDialog(content::BrowserContext* browser_context,
                                 const std::string& email,
                                 const GURL& url) {
  HideDialog();
  // The dialog delegate will be deleted when the widget closes. The created
  // WebView's lifetime is managed by the delegate.
  delegate_ = new UserManagerProfileDialogDelegate(
      this, new views::WebView(browser_context), email, url);
  gfx::NativeView parent = g_user_manager_view->GetWidget()->GetNativeView();
  views::DialogDelegate::CreateDialogWidget(delegate_, nullptr, parent);
  delegate_->GetWidget()->Show();
}

void UserManagerView::HideDialog() {
  if (delegate_) {
    delegate_->CloseDialog();
    DCHECK(!delegate_);
  }
}

void UserManagerView::OnDialogDestroyed() {
  delegate_ = nullptr;
}

void UserManagerView::Init(Profile* system_profile, const GURL& url) {
  web_view_ = new views::WebView(system_profile);
  web_view_->set_allow_accelerators(true);
  AddChildView(web_view_);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddAccelerator(ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN));
  AddAccelerator(ui::Accelerator(ui::VKEY_F4, ui::EF_ALT_DOWN));

  // If the user manager is being displayed from an existing profile, use
  // its last active browser to determine where the user manager should be
  // placed.  This is used so that we can center the dialog on the correct
  // monitor in a multiple-monitor setup.
  //
  // If the last active profile is empty (for example, starting up chrome
  // when all existing profiles are locked), not loaded (for example, if guest
  // was set after locking the only open profile) or we can't find an active
  // browser, bounds will remain empty and the user manager will be centered on
  // the default monitor by default.
  //
  // Note the profile is accessed via GetProfileByPath(GetLastUsedProfileDir())
  // instead of GetLastUsedProfile().  If the last active profile isn't loaded,
  // the latter may try to synchronously load it, which can only be done on a
  // thread where disk IO is allowed.
  gfx::Rect bounds;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const base::FilePath& last_used_profile_path =
      profile_manager->GetLastUsedProfileDir(profile_manager->user_data_dir());
  Profile* profile = profile_manager->GetProfileByPath(last_used_profile_path);
  if (profile) {
    Browser* browser = chrome::FindLastActiveWithProfile(profile);
    if (browser) {
      gfx::NativeView native_view =
          views::Widget::GetWidgetForNativeWindow(
              browser->window()->GetNativeWindow())->GetNativeView();
      bounds = display::Screen::GetScreen()
                   ->GetDisplayNearestView(native_view)
                   .work_area();
      bounds.ClampToCenteredSize(gfx::Size(UserManager::kWindowWidth,
                                           UserManager::kWindowHeight));
    }
  }

  views::Widget::InitParams params =
      GetDialogWidgetInitParams(this, nullptr, nullptr, bounds);
  (new views::Widget)->Init(params);

  // Since the User Manager can be the only top level window, we don't
  // want to accidentally quit all of Chrome if the user is just trying to
  // unfocus the selected pod in the WebView.
  GetDialogClientView()->RemoveAccelerator(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

#if defined(OS_WIN)
  // Set the app id for the task manager to the app id of its parent
  ui::win::SetAppIdForWindow(
      shell_integration::win::GetChromiumModelIdForProfile(
          system_profile->GetPath()),
      views::HWNDForWidget(GetWidget()));
#endif

  web_view_->LoadInitialURL(url);
  content::RenderWidgetHostView* rwhv =
      web_view_->GetWebContents()->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetBackgroundColor(profiles::kUserManagerBackgroundColor);

  GetWidget()->Show();
  web_view_->RequestFocus();
}

void UserManagerView::LogTimeToOpen() {
  if (user_manager_started_showing_ == base::Time())
    return;

  ProfileMetrics::LogTimeToOpenUserManager(
      base::Time::Now() - user_manager_started_showing_);
  user_manager_started_showing_ = base::Time();
}

bool UserManagerView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  int key = accelerator.key_code();
  int modifier = accelerator.modifiers();
  DCHECK((key == ui::VKEY_W && modifier == ui::EF_CONTROL_DOWN) ||
         (key == ui::VKEY_F4 && modifier == ui::EF_ALT_DOWN));
  GetWidget()->Close();
  return true;
}

gfx::Size UserManagerView::CalculatePreferredSize() const {
  return gfx::Size(UserManager::kWindowWidth, UserManager::kWindowHeight);
}

bool UserManagerView::CanResize() const {
  return true;
}

bool UserManagerView::CanMaximize() const {
  return true;
}

bool UserManagerView::CanMinimize() const {
  return true;
}

base::string16 UserManagerView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

int UserManagerView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void UserManagerView::WindowClosing() {
  // Now that the window is closed, we can allow a new one to be opened.
  // (WindowClosing comes in asynchronously from the call to Close() and we
  // may have already opened a new instance).
  if (g_user_manager_view == this)
    g_user_manager_view = NULL;
}

bool UserManagerView::ShouldUseCustomFrame() const {
  return false;
}

void UserManagerView::DisplayErrorMessage() {
  if (delegate_)
    delegate_->DisplayErrorMessage();
}

void UserManagerView::SetSigninProfilePath(const base::FilePath& profile_path) {
  signin_profile_path_ = profile_path;
}

base::FilePath UserManagerView::GetSigninProfilePath() {
  return signin_profile_path_;
}
