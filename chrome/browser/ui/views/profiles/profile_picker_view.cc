// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view.h"

#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_feature_promo_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_flow_controller.h"
#include "chrome/browser/ui/webui/signin/profile_picker_handler.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/views/profiles/first_run_flow_controller_dice.h"
#include "chrome/browser/ui/views/profiles/profile_picker_dice_sign_in_toolbar.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/shell_integration_win.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/views/profiles/first_run_flow_controller_lacros.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/shell_integration_linux.h"
#endif

namespace {

ProfilePickerView* g_profile_picker_view = nullptr;
base::OnceClosure* g_profile_picker_opened_callback_for_testing = nullptr;

constexpr int kWindowTitleId = IDS_PRODUCT_NAME;

constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 758;
constexpr float kMaxRatioOfWorkArea = 0.9;

constexpr int kSupportedAcceleratorCommands[] = {
    IDC_CLOSE_TAB,  IDC_CLOSE_WINDOW,    IDC_EXIT,
    IDC_FULLSCREEN, IDC_MINIMIZE_WINDOW, IDC_BACK,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    IDC_RELOAD
#endif
};

class ProfilePickerWidget : public views::Widget {
 public:
  explicit ProfilePickerWidget(ProfilePickerView* profile_picker_view)
      : profile_picker_view_(profile_picker_view) {
    views::Widget::InitParams params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
    params.delegate = profile_picker_view_;
#if BUILDFLAG(IS_LINUX)
    params.wm_class_name = shell_integration_linux::GetProgramClassName();
    params.wm_class_class = shell_integration_linux::GetProgramClassClass();
    params.wayland_app_id = params.wm_class_class;
#endif
    Init(std::move(params));
  }
  ~ProfilePickerWidget() override = default;

 private:
  const raw_ptr<ProfilePickerView, DanglingUntriaged> profile_picker_view_;
};

// Returns whether the current flow is part of the classic profile picker flow.
// Checking this should become eventually unnecessary as flows move away from
// using static calls and global variables, and keep calls to native contained
// within their own steps. See crbug.com/1359352.
bool IsClassicProfilePickerFlow(const ProfilePicker::Params& params) {
  // TODO(crbug.com/40237764): Implement more use cases outside of the classic
  // profile picker flow. e.g.: kLacrosSelectAvailableAccount.
  switch (params.entry_point()) {
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuAddNewProfile:
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOnStartup:
    case ProfilePicker::EntryPoint::kProfileMenuManageProfiles:
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile:
    case ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcess:
    case ProfilePicker::EntryPoint::kProfileLocked:
    case ProfilePicker::EntryPoint::kUnableToCreateBrowser:
    case ProfilePicker::EntryPoint::kBackgroundModeManager:
    case ProfilePicker::EntryPoint::kProfileIdle:
    case ProfilePicker::EntryPoint::kLacrosSelectAvailableAccount:
    case ProfilePicker::EntryPoint::kOnStartupNoProfile:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcessNoProfile:
      return true;
    case ProfilePicker::EntryPoint::kLacrosPrimaryProfileFirstRun:
    case ProfilePicker::EntryPoint::kFirstRun:
      return false;
  }
}

void ClearLockedProfilesFirstBrowserKeepAlive() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const std::vector<Profile*> loaded_profiles =
      profile_manager->GetLoadedProfiles();
  for (Profile* profile : loaded_profiles) {
    ProfileAttributesEntry* entry =
        profile_manager->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath());
    if (entry && entry->IsSigninRequired()) {
      profile_manager->ClearFirstBrowserWindowKeepAlive(profile);
    }
  }
}

}  // namespace

// static
void ProfilePicker::Show(Params&& params) {
  // Re-open with new params if necessary.
  if (g_profile_picker_view && g_profile_picker_view->MaybeReopen(params)) {
    return;
  }

  if (g_profile_picker_view) {
    g_profile_picker_view->UpdateParams(std::move(params));
  } else {
    g_profile_picker_view = new ProfilePickerView(std::move(params));
  }
  g_profile_picker_view->Display();
}

// static
GURL ProfilePicker::GetOnSelectProfileTargetUrl() {
  if (g_profile_picker_view) {
    return g_profile_picker_view->GetOnSelectProfileTargetUrl();
  }
  return GURL();
}

// static
base::FilePath ProfilePicker::GetSwitchProfilePath() {
  if (g_profile_picker_view) {
    return g_profile_picker_view->GetProfilePickerFlowController()
        ->GetSwitchProfilePathOrEmpty();
  }
  return base::FilePath();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// static
void ProfilePicker::SwitchToDiceSignIn(
    ProfilePicker::ProfileInfo profile_info,
    base::OnceCallback<void(bool)> switch_finished_callback) {
  if (g_profile_picker_view) {
    g_profile_picker_view->SwitchToDiceSignIn(
        std::move(profile_info), std::move(switch_finished_callback));
  }
}

// static
void ProfilePicker::SwitchToReauth(
    Profile* profile,
    base::OnceCallback<void(const ForceSigninUIError&)> on_error_callback) {
  if (g_profile_picker_view) {
    g_profile_picker_view->SwitchToReauth(profile,
                                          std::move(on_error_callback));
  }
}
#endif

// static
void ProfilePicker::SwitchToSignedOutPostIdentityFlow(
    std::optional<SkColor> profile_color,
    base::TimeTicks profile_picked_time_on_startup,
    base::OnceCallback<void(bool)> switch_finished_callback) {
  if (g_profile_picker_view) {
    g_profile_picker_view->SwitchToSignedOutPostIdentityFlow(
        profile_color, profile_picked_time_on_startup,
        std::move(switch_finished_callback));
  }
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// static
void ProfilePicker::SwitchToSignedInFlow(std::optional<SkColor> profile_color,
                                         Profile* signed_in_profile) {
  if (g_profile_picker_view) {
    g_profile_picker_view->SwitchToSignedInFlow(
        signed_in_profile, profile_color,
        content::WebContents::Create(
            content::WebContents::CreateParams(signed_in_profile)));
  }
}
#endif

// static
void ProfilePicker::CancelSignedInFlow() {
  if (g_profile_picker_view) {
    g_profile_picker_view->flow_controller_.get()->CancelPostSignInFlow();
  }
}

// static
base::FilePath ProfilePicker::GetPickerProfilePath() {
  return ProfileManager::GetSystemProfilePath();
}

// static
void ProfilePicker::ShowDialog(Profile* profile, const GURL& url) {
  if (g_profile_picker_view) {
    g_profile_picker_view->ShowDialog(profile, url);
  }
}

// static
void ProfilePicker::HideDialog() {
  if (g_profile_picker_view) {
    g_profile_picker_view->HideDialog();
  }
}

// static
void ProfilePicker::Hide() {
  if (g_profile_picker_view) {
    g_profile_picker_view->Clear();
  }
}

// static
bool ProfilePicker::IsOpen() {
  return g_profile_picker_view;
}

// static
bool ProfilePicker::IsFirstRunOpen() {
  return ProfilePicker::IsOpen() &&
         (g_profile_picker_view->params_.entry_point() ==
              ProfilePicker::EntryPoint::kLacrosPrimaryProfileFirstRun ||
          g_profile_picker_view->params_.entry_point() ==
              ProfilePicker::EntryPoint::kFirstRun);
}

bool ProfilePicker::IsActive() {
  if (!IsOpen()) {
    return false;
  }

#if BUILDFLAG(IS_MAC)
  return g_profile_picker_view->GetWidget() &&
         g_profile_picker_view->GetWidget()->IsVisible();
#else
  return g_profile_picker_view->GetWidget()->IsActive();
#endif
}

// static
views::WebView* ProfilePicker::GetWebViewForTesting() {
  if (!g_profile_picker_view) {
    return nullptr;
  }
  return g_profile_picker_view->web_view_;
}

// static
views::View* ProfilePicker::GetViewForTesting() {
  return g_profile_picker_view;
}

// static
void ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
    base::OnceClosure callback) {
  DCHECK(!g_profile_picker_opened_callback_for_testing);
  DCHECK(!callback.is_null());
  g_profile_picker_opened_callback_for_testing =
      new base::OnceClosure(std::move(callback));
}

// static
void ProfilePicker::ShowDialogAndDisplayErrorMessage(Profile* profile) {
  if (!ProfilePicker::IsActive()) {
    return;
  }

  GURL url(chrome::kChromeUISigninErrorURL);
  url = AddFromProfilePickerURLParameter(url);
  ProfilePicker::ShowDialog(profile, url);
  return;
}

// ProfilePickerForceSigninDialog
// -------------------------------------------------------------

// static
void ProfilePickerForceSigninDialog::ShowReauthDialog(
    Profile* profile,
    const std::string& email) {
  DCHECK(signin_util::IsForceSigninEnabled());
  if (!ProfilePicker::IsActive()) {
    return;
  }
  GURL url = signin::GetEmbeddedReauthURLWithEmail(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::Reason::kReauthentication, email);
  url = AddFromProfilePickerURLParameter(url);
  ProfilePicker::ShowDialog(profile, url);
}

// static
void ProfilePickerForceSigninDialog::ShowForceSigninDialog(Profile* profile) {
  DCHECK(signin_util::IsForceSigninEnabled());
  if (!ProfilePicker::IsActive()) {
    return;
  }

  GURL url = signin::GetEmbeddedPromoURL(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::Reason::kForcedSigninPrimaryAccount, true);
  url = AddFromProfilePickerURLParameter(url);

  ProfilePicker::ShowDialog(profile, url);
}

// static
void ProfilePickerForceSigninDialog::DisplayErrorMessage() {
  DCHECK(signin_util::IsForceSigninEnabled());
  if (g_profile_picker_view) {
    g_profile_picker_view->DisplayErrorMessage();
  }
}

// ProfilePickerView::NavigationFinishedObserver ------------------------------

ProfilePickerView::NavigationFinishedObserver::NavigationFinishedObserver(
    const GURL& requested_url,
    base::OnceClosure closure,
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      requested_url_(requested_url),
      closure_(std::move(closure)) {}

ProfilePickerView::NavigationFinishedObserver::~NavigationFinishedObserver() =
    default;

void ProfilePickerView::NavigationFinishedObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!closure_ || !navigation_handle->HasCommitted()) {
    return;
  }

  if (navigation_handle->GetRedirectChain()[0] != requested_url_) {
    // Don't notify if the URL for the finishing navigation does not match.
    // The navigation may have been replaced by a new one. We are mindful to
    // allow redirections, which are necessary for example for Gaia sign-in
    // pages (see crbug.com/1430681).
    return;
  }

  if (navigation_handle->IsErrorPage() &&
      requested_url_.SchemeIs(content::kChromeUIScheme)) {
    // We observed some cases where the navigation to the intended page fails
    // (see crbug.com/1442159).
    // Loading the wrong URL may lead to crashes if we are expecting a certain
    // WebUI page to be loaded in the web contents. For these cases we will not
    // notify of the finished navigation to avoid crashing, but this negatively
    // affects the user experience anyway.
    // TODO(crbug.com/40911651): Improve the user experience for this error.
    base::debug::DumpWithoutCrashing();
    return;
  }

  std::move(closure_).Run();
}

// ProfilePickerView ----------------------------------------------------------

void ProfilePickerView::UpdateParams(ProfilePicker::Params&& params) {
  DCHECK(params_.CanReusePickerWindow(params));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Cancel any flow that was in progress.
  params_.NotifyAccountSelected(std::string());
  params_.NotifyFirstRunExited(ProfilePicker::FirstRunExitStatus::kQuitEarly);
#endif

  params_ = std::move(params);
}

void ProfilePickerView::DisplayErrorMessage() {
  dialog_host_.DisplayErrorMessage();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ProfilePickerView::NotifyAccountSelected(const std::string& gaia_id) {
  params_.NotifyAccountSelected(gaia_id);
}
#endif

void ProfilePickerView::ShowScreen(
    content::WebContents* contents,
    const GURL& url,
    base::OnceClosure navigation_finished_closure) {
  base::ScopedClosureRunner finish_init_runner;
  if (state_ == kInitializing) {
    finish_init_runner.ReplaceClosure(
        base::BindOnce(&ProfilePickerView::FinishInit, base::Unretained(this)));
  }

  if (url.is_empty()) {
    DCHECK(!navigation_finished_closure);
    ShowScreenFinished(contents);
    return;
  }

  contents->GetController().LoadURL(url, content::Referrer(),
                                    ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                    std::string());

  // Special-case the first ever screen to make sure the WebView has a contents
  // assigned in the moment when it gets displayed. This avoids a black flash on
  // Win (and potentially other GPU artifacts on other platforms). The rest of
  // the work can still be done asynchronously in ShowScreenFinished().
  if (web_view_->GetWebContents() == nullptr) {
    web_view_->SetWebContents(contents);
  }

  // Binding as Unretained as `this` outlives member
  // `show_screen_finished_observer_`. If ShowScreen gets called twice in a
  // short period of time, the first callback may never get called as the first
  // observer gets destroyed here or later in ShowScreenFinished(). This is okay
  // as all the previous values get replaced by the new values.
  show_screen_finished_observer_ = std::make_unique<NavigationFinishedObserver>(
      url,
      base::BindOnce(&ProfilePickerView::ShowScreenFinished,
                     base::Unretained(this), contents,
                     std::move(navigation_finished_closure)),
      contents);

  if (!GetWidget()->IsVisible()) {
    GetWidget()->Show();
  }
}

void ProfilePickerView::ShowScreenInPickerContents(
    const GURL& url,
    base::OnceClosure navigation_finished_closure) {
  ShowScreen(contents_.get(), url, std::move(navigation_finished_closure));
}

void ProfilePickerView::Clear() {
  TRACE_EVENT1("browser,startup", "ProfilePickerView::Clear", "state", state_);
  if (state_ == kClosing) {
    return;
  }

  state_ = kClosing;

  if (GetWidget()) {
    GetWidget()->Close();
    return;
  }

  WindowClosing();
  // TODO(crbug.com/40232473): Here we set owned by widget to ensure the
  // DeleteDelegate() call deletes this instance. Once the full migration to
  // "client owns delegate" is done, this will need to change.
  SetOwnedByWidget(true);
  DeleteDelegate();
}

bool ProfilePickerView::ShouldUseDarkColors() const {
  return GetNativeTheme()->ShouldUseDarkColors();
}

content::WebContents* ProfilePickerView::GetPickerContents() const {
  return contents_.get();
}

content::WebContentsDelegate* ProfilePickerView::GetWebContentsDelegate() {
  return this;
}

web_modal::WebContentsModalDialogHost*
ProfilePickerView::GetWebContentsModalDialogHost() {
  return this;
}

void ProfilePickerView::Reset(StepSwitchFinishedCallback callback) {
  flow_controller_->Reset(std::move(callback));
}

void ProfilePickerView::SwitchToSignedOutPostIdentityFlow(
    std::optional<SkColor> profile_color,
    base::TimeTicks profile_picked_time_on_startup,
    base::OnceCallback<void(bool)> switch_finished_callback) {
  size_t icon_index = profiles::GetPlaceholderAvatarIndex();

  ProfileManager::CreateMultiProfileAsync(
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .ChooseNameForNewProfile(icon_index),
      icon_index, /*is_hidden=*/true,
      base::BindOnce(&ProfilePickerView::OnLocalProfileInitialized,
                     weak_ptr_factory_.GetWeakPtr(), profile_color,
                     profile_picked_time_on_startup,
                     std::move(switch_finished_callback)));
}

void ProfilePickerView::OnLocalProfileInitialized(
    std::optional<SkColor> profile_color,
    base::TimeTicks profile_picked_time_on_startup,
    base::OnceCallback<void(bool)> switch_finished_callback,
    Profile* profile) {
  if (!profile) {
    NOTREACHED_IN_MIGRATION() << "Local fail in creating new profile";
    std::move(switch_finished_callback).Run(false);
    return;
  }
  CHECK(!signin_util::IsForceSigninEnabled(), base::NotFatalUntil::M127);

  // Apply a new color to the profile or use the default theme.
  // TODO(b/328587059): Share the theme color logic with the same code in
  // `profile_picker_flow_controller.cc`.
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile);
  if (profile_color.has_value()) {
    theme_service->SetUserColorAndBrowserColorVariant(
        *profile_color, ui::mojom::BrowserColorVariant::kTonalSpot);
  } else {
    theme_service->UseDefaultTheme();
  }

  // TODO(crbug.com/40209493): Add shortcut creation.
  // Skip the FRE for this profile as sign-in was offered as part of the flow.
  profile->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, true);
  GetProfilePickerFlowController()->SwitchToSignedOutPostIdentityFlow(
      profile,
      PostHostClearedCallback(base::BindOnce(
          &ProfilePickerView::ShowLocalProfileCustomization,
          weak_ptr_factory_.GetWeakPtr(), profile_picked_time_on_startup)),
      std::move(switch_finished_callback));
}

void ProfilePickerView::ShowLocalProfileCustomization(
    base::TimeTicks profile_picked_time_on_startup,
    Browser* browser) {
  if (!browser) {
    // TODO(crbug.com/40242414): Make sure we do something or log an error if
    // opening a browser window was not possible.
    return;
  }

  DCHECK(browser->window());
  Profile* profile = browser->profile();

  TRACE_EVENT1("browser", "ProfilePickerView::ShowLocalProfileCustomization",
               "profile_path", profile->GetPath().AsUTF8Unsafe());

  if (!profile_picked_time_on_startup.is_null()) {
    ProfilePickerHandler::BeginFirstWebContentsProfiling(
        browser, profile_picked_time_on_startup);
  }

  browser->signin_view_controller()->ShowModalProfileCustomizationDialog(
      /*is_local_profile_creation=*/true);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProfilePickerView::SetNativeToolbarVisible(bool visible) {
  if (!visible) {
    toolbar_->SetVisible(false);
    return;
  }

  if (toolbar_->children().empty()) {
    toolbar_->BuildToolbar(
        base::BindRepeating(&ProfilePickerView::NavigateBack,
                            // Binding as Unretained as `this` is the
                            // `toolbar_`'s parent and outlives it.
                            base::Unretained(this)));
  }
  toolbar_->SetVisible(true);
}

bool ProfilePickerView::IsNativeToolbarVisibleForTesting() const {
  return toolbar_->GetVisible();
}

SkColor ProfilePickerView::GetPreferredBackgroundColor() const {
  return GetColorProvider()->GetColor(kColorToolbar);
}
#endif

bool ProfilePickerView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // Forward the keyboard event to AcceleratorPressed() through the
  // FocusManager.
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

bool ProfilePickerView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

gfx::NativeView ProfilePickerView::GetHostView() const {
  return GetWidget()->GetNativeView();
}

gfx::Point ProfilePickerView::GetDialogPosition(const gfx::Size& size) {
  gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
  return gfx::Point(std::max(0, (widget_size.width() - size.width()) / 2), 0);
}

gfx::Size ProfilePickerView::GetMaximumDialogSize() {
  return GetWidget()->GetWindowBoundsInScreen().size();
}

void ProfilePickerView::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {}

void ProfilePickerView::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {}

ProfilePickerView::ProfilePickerView(ProfilePicker::Params&& params)
    : keep_alive_(KeepAliveOrigin::USER_MANAGER_VIEW,
                  KeepAliveRestartOption::DISABLED),
      params_(std::move(params)) {
  // Setup the WidgetDelegate.
  SetHasWindowSizeControls(true);
  SetTitle(kWindowTitleId);

  ConfigureAccelerators();

  // Record creation metrics.
  base::UmaHistogramEnumeration("ProfilePicker.Shown", params_.entry_point());
  if (params_.entry_point() == ProfilePicker::EntryPoint::kOnStartup) {
    DCHECK(creation_time_on_startup_.is_null());
    creation_time_on_startup_ = base::TimeTicks::Now();
    base::UmaHistogramTimes(
        "ProfilePicker.StartupTime.BeforeCreation",
        creation_time_on_startup_ -
            startup_metric_utils::GetCommon().MainEntryPointTicks());
  }
}

ProfilePickerView::~ProfilePickerView() {
  if (contents_) {
    contents_->SetDelegate(nullptr);
  }
}

bool ProfilePickerView::MaybeReopen(ProfilePicker::Params& params) {
  // Re-open if already closing or if the picker cannot be reused with `params`.
  if (state_ != kClosing && params.CanReusePickerWindow(params_)) {
    return false;
  }

  restart_on_window_closing_ =
      base::BindOnce(&ProfilePicker::Show, std::move(params));
  // No-op if already closing.
  ProfilePicker::Hide();
  return true;
}

void ProfilePickerView::Display() {
  DCHECK_NE(state_, kClosing);
  TRACE_EVENT2("browser,startup", "ProfilePickerView::Display", "entry_point",
               params_.entry_point(), "state", state_);

  if (state_ == kNotStarted) {
    state_ = kInitializing;
    // Build the layout synchronously before creating the picker profile to
    // simplify tests.
    BuildLayout();
    g_browser_process->profile_manager()->CreateProfileAsync(
        params_.profile_path(),
        base::BindOnce(&ProfilePickerView::OnPickerProfileCreated,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (state_ == kInitializing) {
    return;
  }

  GetWidget()->Activate();
}

void ProfilePickerView::OnPickerProfileCreated(Profile* picker_profile) {
  TRACE_EVENT1(
      "browser,startup", "ProfilePickerView::OnPickerProfileCreated",
      "profile_path",
      (picker_profile ? picker_profile->GetPath().AsUTF8Unsafe() : ""));
  DCHECK(picker_profile);
  Init(picker_profile);

  InitializeFeaturePromo(picker_profile);
}

void ProfilePickerView::Init(Profile* picker_profile) {
  DCHECK_EQ(state_, kInitializing);
  TRACE_EVENT1(
      "browser,startup", "ProfilePickerView::Init", "profile_path",
      (picker_profile ? picker_profile->GetPath().AsUTF8Unsafe() : ""));
  contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(picker_profile));
  contents_->SetDelegate(this);

  // Destroy the System Profile when the ProfilePickerView is closed (assuming
  // its refcount hits 0). We need to use GetOriginalProfile() here because
  // |profile_picker| is an OTR Profile, and ScopedProfileKeepAlive only
  // supports non-OTR Profiles. Trying to acquire a keepalive on the OTR Profile
  // would trigger a DCHECK.
  //
  // TODO(crbug.com/40159237): Once OTR Profiles use refcounting, remove the
  // call to GetOriginalProfile(). The OTR Profile will hold a keepalive on the
  // regular Profile, so the ownership model will be more straightforward.
  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      picker_profile->GetOriginalProfile(),
      ProfileKeepAliveOrigin::kProfilePickerView);

  // The `FlowController` is created before the widget so it can be used to
  // determine certain aspects of it. E.g. see `GetAccessibleWindowTitle()`.
  flow_controller_ = CreateFlowController(picker_profile, GetClearClosure());

  // The widget is owned by the native widget.
  new ProfilePickerWidget(this);

#if BUILDFLAG(IS_WIN)
  // Set the app id for the user manager to the app id of its parent.
  ui::win::SetAppIdForWindow(
      shell_integration::win::GetAppUserModelIdForBrowser(
          picker_profile->GetPath()),
      views::HWNDForWidget(GetWidget()));
#endif

  DCHECK(flow_controller_);
  flow_controller_->Init();
}

void ProfilePickerView::FinishInit() {
  DCHECK_EQ(kInitializing, state_);
  state_ = kDisplayed;

  if (IsClassicProfilePickerFlow(params_)) {
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kBrowserProfilePickerShown, true);
  }

  if (params_.entry_point() == ProfilePicker::EntryPoint::kOnStartup) {
    DCHECK(!creation_time_on_startup_.is_null());
    base::UmaHistogramTimes("ProfilePicker.StartupTime.WebViewCreated",
                            base::TimeTicks::Now() - creation_time_on_startup_);
  }

  if (g_profile_picker_opened_callback_for_testing) {
    std::move(*g_profile_picker_opened_callback_for_testing).Run();
    delete g_profile_picker_opened_callback_for_testing;
    g_profile_picker_opened_callback_for_testing = nullptr;
  }
}

std::unique_ptr<ProfileManagementFlowController>
ProfilePickerView::CreateFlowController(Profile* picker_profile,
                                        ClearHostClosure clear_host_callback) {
  if (params_.entry_point() ==
          ProfilePicker::EntryPoint::kLacrosPrimaryProfileFirstRun ||
      params_.entry_point() == ProfilePicker::EntryPoint::kFirstRun) {
    auto first_run_exited_callback =
        base::BindOnce(&ProfilePicker::Params::NotifyFirstRunExited,
                       // Unretained ok because the controller is owned
                       // by this through `initialized_steps_`.
                       base::Unretained(&params_));
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    return std::make_unique<FirstRunFlowControllerLacros>(
        /*host=*/this, std::move(clear_host_callback), picker_profile,
        std::move(first_run_exited_callback));
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)
    return std::make_unique<FirstRunFlowControllerDice>(
        /*host=*/this, std::move(clear_host_callback), picker_profile,
        std::move(first_run_exited_callback));
#endif
  }

  DCHECK(IsClassicProfilePickerFlow(params_));
  return std::make_unique<ProfilePickerFlowController>(
      /*host=*/this, std::move(clear_host_callback), params_.entry_point());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProfilePickerView::SwitchToDiceSignIn(
    ProfilePicker::ProfileInfo profile_info,
    base::OnceCallback<void(bool)> switch_finished_callback) {
  // TODO(crbug.com/40237765): Consider having forced signin as separate step
  // controller for `Step::kAccountSelection`.
  if (signin_util::IsForceSigninEnabled() &&
      !base::FeatureList::IsEnabled(kForceSigninFlowInProfilePicker)) {
    SwitchToForcedSignIn(std::move(switch_finished_callback));
    return;
  }

  GetProfilePickerFlowController()->SwitchToDiceSignIn(
      std::move(profile_info), std::move(switch_finished_callback));
}

void ProfilePickerView::SwitchToForcedSignIn(
    base::OnceCallback<void(bool)> switch_finished_callback) {
  DCHECK(signin_util::IsForceSigninEnabled());
  size_t icon_index = profiles::GetPlaceholderAvatarIndex();
  ProfileManager::CreateMultiProfileAsync(
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .ChooseNameForNewProfile(icon_index),
      icon_index, /*is_hidden=*/true,
      base::BindOnce(&ProfilePickerView::OnProfileForDiceForcedSigninCreated,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(switch_finished_callback)));
}

void ProfilePickerView::OnProfileForDiceForcedSigninCreated(
    base::OnceCallback<void(bool)> switch_finished_callback,
    Profile* profile) {
  DCHECK(signin_util::IsForceSigninEnabled());
  if (!profile) {
    std::move(switch_finished_callback).Run(false);
    return;
  }

  std::move(switch_finished_callback).Run(true);
  ProfilePickerForceSigninDialog::ShowForceSigninDialog(profile);
}

void ProfilePickerView::SwitchToReauth(
    Profile* profile,
    base::OnceCallback<void(const ForceSigninUIError&)> on_error_callback) {
  GetProfilePickerFlowController()->SwitchToReauth(
      profile, std::move(on_error_callback));
}
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ProfilePickerView::SwitchToSignedInFlow(
    Profile* signed_in_profile,
    std::optional<SkColor> profile_color,
    std::unique_ptr<content::WebContents> contents) {
  DCHECK(!signin_util::IsForceSigninEnabled());
  GetProfilePickerFlowController()->SwitchToPostSignIn(
      signed_in_profile,
      IdentityManagerFactory::GetForProfile(signed_in_profile)
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin),
      profile_color, std::move(contents));
}
#endif

void ProfilePickerView::WindowClosing() {
  // If a profile is locked, it might have been loaded and it's first browser
  // will never be created, we need to remove it's equivalent
  // `ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow` to be able to
  // delete the profile.
  ClearLockedProfilesFirstBrowserKeepAlive();

  views::WidgetDelegateView::WindowClosing();
  // Now that the window is closed, we can allow a new one to be opened.
  // (WindowClosing comes in asynchronously from the call to Close() and we
  // may have already opened a new instance).
  if (g_profile_picker_view == this) {
    g_profile_picker_view = nullptr;
  }

  // Show a new profile window if it has been requested while the current window
  // was closing.
  if (state_ == kClosing && restart_on_window_closing_) {
    std::move(restart_on_window_closing_).Run();
  }
}

views::ClientView* ProfilePickerView::CreateClientView(views::Widget* widget) {
  return new views::ClientView(widget, TransferOwnershipOfContentsView());
}

views::View* ProfilePickerView::GetContentsView() {
  return this;
}

std::u16string ProfilePickerView::GetAccessibleWindowTitle() const {
  if (web_view_ && web_view_->GetWebContents() &&
      !web_view_->GetWebContents()->GetTitle().empty()) {
    return web_view_->GetWebContents()->GetTitle();
  }

  auto flow_fallback_title =
      flow_controller_->GetFallbackAccessibleWindowTitle();
  if (!flow_fallback_title.empty()) {
    return flow_fallback_title;
  }

  return l10n_util::GetStringUTF16(kWindowTitleId);
}

gfx::Size ProfilePickerView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size preferred_size = gfx::Size(kWindowWidth, kWindowHeight);
  gfx::Size work_area_size = GetWidget()->GetWorkAreaBoundsInScreen().size();
  // Keep the window smaller then |work_area_size| so that it feels more like a
  // dialog then like the actual Chrome window.
  gfx::Size max_dialog_size = ScaleToFlooredSize(
      work_area_size, kMaxRatioOfWorkArea, kMaxRatioOfWorkArea);
  preferred_size.SetToMin(max_dialog_size);
  return preferred_size;
}

gfx::Size ProfilePickerView::GetMinimumSize() const {
  // On small screens, the preferred size may be smaller than the picker
  // minimum size. In that case there will be scrollbars on the picker.
  gfx::Size minimum_size = GetPreferredSize();
  minimum_size.SetToMin(ProfilePickerUI::GetMinimumSize());
  return minimum_size;
}

bool ProfilePickerView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  const auto& iter = accelerator_table_.find(accelerator);
  CHECK(iter != accelerator_table_.end(), base::NotFatalUntil::M130);
  int command_id = iter->second;
  switch (command_id) {
    case IDC_CLOSE_TAB:
    case IDC_CLOSE_WINDOW:
      // kEscKeyPressed is used although that shortcut is disabled (this is
      // Ctrl/Cmd-W instead).
      GetWidget()->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
      break;
    case IDC_EXIT:
      // Stop the browser from re-opening when we close Chrome while
      // in the first run experience.
      params_.NotifyFirstRunExited(
          ProfilePicker::FirstRunExitStatus::kAbandonedFlow);
      chrome::AttemptUserExit();
      break;
    case IDC_FULLSCREEN:
      GetWidget()->SetFullscreen(!GetWidget()->IsFullscreen());
      break;
    case IDC_MINIMIZE_WINDOW:
      GetWidget()->Minimize();
      break;
    case IDC_BACK: {
      NavigateBack();
      break;
    }
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    // Always reload bypassing cache.
    case IDC_RELOAD:
    case IDC_RELOAD_BYPASSING_CACHE:
    case IDC_RELOAD_CLEARING_CACHE:
      flow_controller_->OnReloadRequested();
      break;

#endif
    default:
      NOTREACHED() << "Unexpected command_id: " << command_id;
  }

  return true;
}

bool ProfilePickerView::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  for (const auto& [accelerator_entry, command_id_entry] : accelerator_table_) {
    if (command_id == command_id_entry) {
      *accelerator = accelerator_entry;
      return true;
    }
  }
  return false;
}

void ProfilePickerView::BuildLayout() {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                   views::MaximumFlexSizeRule::kUnbounded));

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  auto toolbar = std::make_unique<ProfilePickerDiceSignInToolbar>();
  toolbar_ = AddChildView(std::move(toolbar));
  // Toolbar gets built and set visible once we it's needed for the Dice signin.
  SetNativeToolbarVisible(false);
#endif

  auto web_view = std::make_unique<views::WebView>();
  web_view->set_allow_accelerators(true);
  web_view_ = AddChildView(std::move(web_view));
}

void ProfilePickerView::ShowScreenFinished(
    content::WebContents* contents,
    base::OnceClosure navigation_finished_closure) {
  // Stop observing for this (or any previous) navigation.
  if (show_screen_finished_observer_) {
    show_screen_finished_observer_.reset();
  }

  web_view_->SetWebContents(contents);
  contents->Focus();

  if (navigation_finished_closure) {
    std::move(navigation_finished_closure).Run();
  }
}

void ProfilePickerView::NavigateBack() {
  flow_controller_->OnNavigateBackRequested();
}

void ProfilePickerView::ConfigureAccelerators() {
  const std::vector<AcceleratorMapping> accelerator_list(GetAcceleratorList());
  for (const auto& entry : accelerator_list) {
    if (!base::Contains(kSupportedAcceleratorCommands, entry.command_id)) {
      continue;
    }
    ui::Accelerator accelerator(entry.keycode, entry.modifiers);
    accelerator_table_[accelerator] = entry.command_id;
    AddAccelerator(accelerator);
  }

#if BUILDFLAG(IS_MAC)
  // Check Mac-specific accelerators. Note: Chrome does not support dynamic or
  // user-configured accelerators on Mac. Default static accelerators are used
  // instead.
  for (int command_id : kSupportedAcceleratorCommands) {
    ui::Accelerator accelerator;
    bool mac_accelerator_found =
        GetDefaultMacAcceleratorForCommandId(command_id, &accelerator);
    if (mac_accelerator_found) {
      accelerator_table_[accelerator] = command_id;
      AddAccelerator(accelerator);
    }
  }
#endif  // BUILDFLAG(IS_MAC)
}

void ProfilePickerView::InitializeFeaturePromo(Profile* system_profile) {
  feature_engagement::Tracker* const tracker_service =
      feature_engagement::TrackerFactory::GetForBrowserContext(system_profile);
  UserEducationService* const user_education_service =
      UserEducationServiceFactory::GetForBrowserContext(system_profile);

  feature_promo_ = std::make_unique<ProfilePickerFeaturePromoController>(
      tracker_service, user_education_service, g_profile_picker_view);
}

void ProfilePickerView::ShowDialog(Profile* profile, const GURL& url) {
  gfx::NativeView parent = GetWidget()->GetNativeView();
  dialog_host_.ShowDialog(profile, url, parent);
}

void ProfilePickerView::HideDialog() {
  dialog_host_.HideDialog();
}

GURL ProfilePickerView::GetOnSelectProfileTargetUrl() const {
  return params_.on_select_profile_target_url();
}

ProfilePickerFlowController* ProfilePickerView::GetProfilePickerFlowController()
    const {
  DCHECK(IsClassicProfilePickerFlow(params_));
  return static_cast<ProfilePickerFlowController*>(flow_controller_.get());
}

ClearHostClosure ProfilePickerView::GetClearClosure() {
  return ClearHostClosure(base::BindOnce(&ProfilePickerView::Clear,
                                         weak_ptr_factory_.GetWeakPtr()));
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// static
void ProfilePicker::NotifyAccountSelected(const std::string& gaia_id) {
  if (!g_profile_picker_view) {
    return;
  }
  g_profile_picker_view->NotifyAccountSelected(gaia_id);
}
#endif

void ProfilePickerView::ShowForceSigninErrorDialog(
    const ForceSigninUIError& error,
    bool switch_step_success) {
  if (!switch_step_success) {
    return;
  }

  CHECK(signin_util::IsForceSigninEnabled());
  ProfilePickerUI* web_ui = web_view_->GetWebContents()
                                ->GetWebUI()
                                ->GetController()
                                ->GetAs<ProfilePickerUI>();
  web_ui->ShowForceSigninErrorDialog(error);
}

BEGIN_METADATA(ProfilePickerView)
END_METADATA
