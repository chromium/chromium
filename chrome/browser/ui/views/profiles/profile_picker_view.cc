// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/profiles/profile_picker_sign_in_flow_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "chrome/browser/ui/webui/signin/signin_web_dialog_ui.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if defined(OS_WIN)
#include "chrome/browser/shell_integration_win.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if defined(OS_MAC)
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#endif

namespace {

ProfilePickerView* g_profile_picker_view = nullptr;
base::OnceClosure* g_profile_picker_opened_callback_for_testing = nullptr;

constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 758;
constexpr float kMaxRatioOfWorkArea = 0.9;

// Padding of elements in the simple toolbar.
constexpr gfx::Insets kToolbarPadding = gfx::Insets(8);

constexpr base::TimeDelta kExtendedAccountInfoTimeout =
    base::TimeDelta::FromSeconds(10);

constexpr int kSupportedAcceleratorCommands[] = {
    IDC_CLOSE_TAB,       IDC_CLOSE_WINDOW, IDC_EXIT,  IDC_FULLSCREEN,
    IDC_MINIMIZE_WINDOW, IDC_BACK,         IDC_RELOAD};

GURL CreateURLForEntryPoint(ProfilePicker::EntryPoint entry_point) {
  GURL base_url = GURL(chrome::kChromeUIProfilePickerUrl);
  switch (entry_point) {
    case ProfilePicker::EntryPoint::kOnStartup: {
      GURL::Replacements replacements;
      replacements.SetQueryStr(chrome::kChromeUIProfilePickerStartupQuery);
      return base_url.ReplaceComponents(replacements);
    }
    case ProfilePicker::EntryPoint::kProfileMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcess:
    case ProfilePicker::EntryPoint::kProfileLocked:
    case ProfilePicker::EntryPoint::kUnableToCreateBrowser:
    case ProfilePicker::EntryPoint::kBackgroundModeManager:
      return base_url;
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile:
      return base_url.Resolve("new-profile");
  }
}

class ProfilePickerWidget : public views::Widget {
 public:
  explicit ProfilePickerWidget(ProfilePickerView* profile_picker_view)
      : profile_picker_view_(profile_picker_view) {
    views::Widget::InitParams params;
    params.delegate = profile_picker_view_;
    Init(std::move(params));
  }
  ~ProfilePickerWidget() override = default;

  // views::Widget:
  const ui::ThemeProvider* GetThemeProvider() const override {
    return profile_picker_view_->GetThemeProviderForProfileBeingCreated();
  }

 private:
  ProfilePickerView* const profile_picker_view_;
};

class SimpleBackButton : public ToolbarButton {
 public:
  explicit SimpleBackButton(PressedCallback callback)
      : ToolbarButton(std::move(callback)) {
    SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                             ui::EF_MIDDLE_MOUSE_BUTTON);
    SetVectorIcons(vector_icons::kBackArrowIcon, kBackArrowTouchIcon);
    SetTooltipText(l10n_util::GetStringUTF16(
        IDS_PROFILE_PICKER_BACK_BUTTON_SIGN_IN_LABEL));
    // Unlike toolbar buttons, this one should be focusable to make it
    // consistent with other screens of the flow where the back button is part
    // of the page.
    SetFocusBehavior(FocusBehavior::ALWAYS);
  }
  SimpleBackButton(const SimpleBackButton&) = delete;
  SimpleBackButton& operator=(const SimpleBackButton&) = delete;
  ~SimpleBackButton() override = default;
};

}  // namespace

// static
void ProfilePicker::Show(EntryPoint entry_point,
                         const GURL& on_select_profile_target_url) {
  if (!g_profile_picker_view)
    g_profile_picker_view = new ProfilePickerView();
  g_profile_picker_view->set_on_select_profile_target_url(
      on_select_profile_target_url);
  g_profile_picker_view->Display(entry_point);
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
  if (g_profile_picker_view && g_profile_picker_view->sign_in_) {
    return g_profile_picker_view->sign_in_->switch_profile_path();
  }
  return base::FilePath();
}

// static
void ProfilePicker::SwitchToSignIn(
    SkColor profile_color,
    base::OnceCallback<void(bool)> switch_finished_callback) {
  if (g_profile_picker_view) {
    g_profile_picker_view->SwitchToSignIn(profile_color,
                                          std::move(switch_finished_callback));
  }
}

// static
void ProfilePicker::CancelSignIn() {
  if (g_profile_picker_view) {
    g_profile_picker_view->CancelSignIn();
  }
}

// static
void ProfilePicker::ShowDialog(content::BrowserContext* browser_context,
                               const GURL& url,
                               const base::FilePath& profile_path) {
  if (g_profile_picker_view) {
    g_profile_picker_view->ShowDialog(browser_context, url, profile_path);
  }
}

// static
void ProfilePicker::HideDialog() {
  if (g_profile_picker_view) {
    g_profile_picker_view->HideDialog();
  }
}

// static
base::FilePath ProfilePicker::GetForceSigninProfilePath() {
  if (g_profile_picker_view) {
    return g_profile_picker_view->GetForceSigninProfilePath();
  }

  return base::FilePath();
}

// static
void ProfilePicker::Hide() {
  if (g_profile_picker_view)
    g_profile_picker_view->Clear();
}

// static
bool ProfilePicker::IsOpen() {
  return g_profile_picker_view;
}

bool ProfilePicker::IsActive() {
  if (!IsOpen())
    return false;

#if defined(OS_MAC)
  return g_profile_picker_view->GetWidget()->IsVisible();
#else
  return g_profile_picker_view->GetWidget()->IsActive();
#endif
}

// static
views::WebView* ProfilePicker::GetWebViewForTesting() {
  if (!g_profile_picker_view)
    return nullptr;
  return g_profile_picker_view->web_view_;
}

// static
views::View* ProfilePicker::GetViewForTesting() {
  return g_profile_picker_view;
}

// static
views::View* ProfilePicker::GetToolbarForTesting() {
  if (!g_profile_picker_view)
    return nullptr;
  return g_profile_picker_view->toolbar_;
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
void ProfilePicker::SetExtendedAccountInfoTimeoutForTesting(
    base::TimeDelta timeout) {
  if (g_profile_picker_view) {
    g_profile_picker_view->SetExtendedAccountInfoTimeoutForTesting(  // IN-TEST
        timeout);
  }
}

// ProfilePickerForceSigninDialog
// -------------------------------------------------------------

// static
void ProfilePickerForceSigninDialog::ShowReauthDialog(
    content::BrowserContext* browser_context,
    const std::string& email,
    const base::FilePath& profile_path) {
  DCHECK(signin_util::IsForceSigninEnabled());
  if (!ProfilePicker::IsActive())
    return;
  GURL url = signin::GetEmbeddedReauthURLWithEmail(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::Reason::kReauthentication, email);
  ProfilePicker::ShowDialog(browser_context, url, profile_path);
}

// static
void ProfilePickerForceSigninDialog::ShowForceSigninDialog(
    content::BrowserContext* browser_context,
    const base::FilePath& profile_path) {
  DCHECK(signin_util::IsForceSigninEnabled());
  if (!ProfilePicker::IsActive())
    return;

  GURL url = signin::GetEmbeddedPromoURL(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::Reason::kForcedSigninPrimaryAccount, true);

  ProfilePicker::ShowDialog(browser_context, url, profile_path);
}

void ProfilePickerForceSigninDialog::ShowDialogAndDisplayErrorMessage(
    content::BrowserContext* browser_context) {
  DCHECK(signin_util::IsForceSigninEnabled());
  if (!ProfilePicker::IsActive())
    return;

  GURL url(chrome::kChromeUISigninErrorURL);
  ProfilePicker::ShowDialog(browser_context, url, base::FilePath());
  return;
}

// static
void ProfilePickerForceSigninDialog::DisplayErrorMessage() {
  DCHECK(signin_util::IsForceSigninEnabled());
  if (g_profile_picker_view) {
    g_profile_picker_view->DisplayErrorMessage();
  }
}

// static
void ProfilePickerForceSigninDialog::HideDialog() {
  ProfilePicker::HideDialog();
}

// ProfilePickerView::NavigationFinishedObserver ------------------------------

ProfilePickerView::NavigationFinishedObserver::NavigationFinishedObserver(
    const GURL& url,
    base::OnceClosure closure,
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      url_(url),
      closure_(std::move(closure)) {}

ProfilePickerView::NavigationFinishedObserver::~NavigationFinishedObserver() =
    default;

void ProfilePickerView::NavigationFinishedObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!closure_ || navigation_handle->GetURL() != url_ ||
      !navigation_handle->HasCommitted()) {
    return;
  }
  std::move(closure_).Run();
}

// ProfilePickerView ----------------------------------------------------------

const ui::ThemeProvider*
ProfilePickerView::GetThemeProviderForProfileBeingCreated() const {
  if (!sign_in_)
    return nullptr;
  return sign_in_->GetThemeProvider();
}

void ProfilePickerView::DisplayErrorMessage() {
  dialog_host_.DisplayErrorMessage();
}

void ProfilePickerView::ShowScreen(
    content::WebContents* contents,
    const GURL& url,
    bool show_toolbar,
    bool enable_navigating_back,
    base::OnceClosure navigation_finished_closure) {
  if (url.is_empty()) {
    DCHECK(!navigation_finished_closure);
    ShowScreenFinished(contents, show_toolbar, enable_navigating_back);
    return;
  }

  // Make sure to load the url as the last step so that the UI state is
  // coherent upon the NavigationStateChanged notification.
  contents->GetController().LoadURL(url, content::Referrer(),
                                    ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                    std::string());
  // Binding as Unretained as `this` outlives member
  // `show_screen_finished_observer_`. If ShowScreen gets called twice in a
  // short period of time, the first callback may never get called as the first
  // observer gets destroyed here or later in ShowScreenFinished(). This is okay
  // as all the previous values get replaced by the new values.
  show_screen_finished_observer_ = std::make_unique<NavigationFinishedObserver>(
      url,
      base::BindOnce(&ProfilePickerView::ShowScreenFinished,
                     base::Unretained(this), contents, show_toolbar,
                     enable_navigating_back,
                     std::move(navigation_finished_closure)),
      contents);
}

void ProfilePickerView::ShowScreenInSystemContents(
    const GURL& url,
    bool show_toolbar,
    bool enable_navigating_back,
    base::OnceClosure navigation_finished_closure) {
  ShowScreen(system_profile_contents_.get(), url, show_toolbar,
             enable_navigating_back, std::move(navigation_finished_closure));
}

void ProfilePickerView::CreateToolbarBackButton() {
  // The sign-in profile is needed to obtain the ThemeProvider which is needed
  // by ToolbarButton on construction.
  DCHECK(sign_in_->profile());
  auto back_button = std::make_unique<SimpleBackButton>(base::BindRepeating(
      &ProfilePickerView::BackButtonPressed, base::Unretained(this)));
  toolbar_->AddChildView(std::move(back_button));
  UpdateToolbarColor();
}

void ProfilePickerView::Clear() {
  if (state_ == kClosing)
    return;

  if (state_ == kReady) {
    GetWidget()->Close();
    state_ = kClosing;
    return;
  }

  WindowClosing();
  DeleteDelegate();
}

bool ProfilePickerView::ShouldUseDarkColors() const {
  return GetNativeTheme()->ShouldUseDarkColors();
}

bool ProfilePickerView::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // Forward the keyboard event to AcceleratorPressed() through the
  // FocusManager.
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

bool ProfilePickerView::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
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

ProfilePickerView::ProfilePickerView()
    : keep_alive_(KeepAliveOrigin::USER_MANAGER_VIEW,
                  KeepAliveRestartOption::DISABLED),
      extended_account_info_timeout_(kExtendedAccountInfoTimeout) {
  // Setup the WidgetDelegate.
  SetHasWindowSizeControls(true);
  SetTitle(IDS_PRODUCT_NAME);

  ConfigureAccelerators();
  // TODO(crbug.com/1063856): Add |RecordDialogCreation|.
}

ProfilePickerView::~ProfilePickerView() {
  if (system_profile_contents_)
    system_profile_contents_->SetDelegate(nullptr);
}

void ProfilePickerView::Display(ProfilePicker::EntryPoint entry_point) {
  // Record creation metrics.
  base::UmaHistogramEnumeration("ProfilePicker.Shown", entry_point);
  if (entry_point == ProfilePicker::EntryPoint::kOnStartup) {
    DCHECK(creation_time_on_startup_.is_null());
    // Display() is called right after the creation of this object.
    creation_time_on_startup_ = base::TimeTicks::Now();
    base::UmaHistogramTimes("ProfilePicker.StartupTime.BeforeCreation",
                            creation_time_on_startup_ -
                                startup_metric_utils::MainEntryPointTicks());
  }

  if (state_ == kNotStarted) {
    state_ = kInitializing;
    entry_point_ = entry_point;
    // Build the layout synchronously before creating the system profile to
    // simplify tests.
    BuildLayout();

    g_browser_process->profile_manager()->CreateProfileAsync(
        ProfileManager::GetSystemProfilePath(),
        base::BindRepeating(&ProfilePickerView::OnSystemProfileCreated,
                            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (state_ == kInitializing)
    return;

  if (state_ == kClosing) {
    if (!restart_with_entry_point_on_window_closing_.has_value())
      restart_with_entry_point_on_window_closing_ = entry_point;
    return;
  }

  GetWidget()->Activate();
}

void ProfilePickerView::OnSystemProfileCreated(Profile* system_profile,
                                               Profile::CreateStatus status) {
  DCHECK_NE(status, Profile::CREATE_STATUS_LOCAL_FAIL);
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  Init(system_profile);
}

void ProfilePickerView::Init(Profile* system_profile) {
  DCHECK_EQ(state_, kInitializing);
  system_profile_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(system_profile));
  system_profile_contents_->SetDelegate(this);
  // To record metrics using javascript, extensions are needed.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      system_profile_contents_.get());

  // The widget is owned by the native widget.
  new ProfilePickerWidget(this);

#if defined(OS_WIN)
  // Set the app id for the user manager to the app id of its parent.
  ui::win::SetAppIdForWindow(
      shell_integration::win::GetAppUserModelIdForBrowser(
          system_profile->GetPath()),
      views::HWNDForWidget(GetWidget()));
#endif

  ShowScreenInSystemContents(CreateURLForEntryPoint(entry_point_),
                             /*show_toolbar=*/false);
  GetWidget()->Show();
  state_ = kReady;

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kBrowserProfilePickerShown, true);

  if (entry_point_ == ProfilePicker::EntryPoint::kOnStartup) {
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

void ProfilePickerView::SwitchToSignIn(
    SkColor profile_color,
    base::OnceCallback<void(bool)> switch_finished_callback) {
  if (sign_in_) {
    // The profile is already created (the user went back and forth again). No
    // need to create it again.
    std::move(switch_finished_callback).Run(true);
    // The color might be different for the second time (as the user could go to
    // the local customization flow and change the color there) so update it.
    sign_in_->SetProfileColor(profile_color);
    // Do not load any url because the desired sign-in screen is still loaded in
    // `sign_in_->contents`.
    ShowScreen(sign_in_->contents(), GURL(),
               /*show_toolbar=*/true);
    return;
  }

  size_t icon_index = profiles::GetPlaceholderAvatarIndex();
  // Silently create the new profile for browsing on GAIA (so that the sign-in
  // cookies are stored in the right profile).
  ProfileManager::CreateMultiProfileAsync(
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .ChooseNameForNewProfile(icon_index),
      icon_index, /*is_hidden=*/true,
      base::BindRepeating(
          &ProfilePickerView::OnProfileForSigninCreated,
          weak_ptr_factory_.GetWeakPtr(), profile_color,
          base::Owned(std::make_unique<base::OnceCallback<void(bool)>>(
              std::move(switch_finished_callback)))));
}

void ProfilePickerView::CancelSignIn() {
  DCHECK(sign_in_);

  g_browser_process->profile_manager()->ScheduleProfileForDeletion(
      sign_in_->profile()->GetPath(), base::DoNothing());

  switch (entry_point_) {
    case ProfilePicker::EntryPoint::kOnStartup:
    case ProfilePicker::EntryPoint::kProfileMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcess:
    case ProfilePicker::EntryPoint::kProfileLocked:
    case ProfilePicker::EntryPoint::kUnableToCreateBrowser:
    case ProfilePicker::EntryPoint::kBackgroundModeManager: {
      // Navigate to the very beginning which is guaranteed to be the profile
      // picker.
      system_profile_contents_->GetController().GoToIndex(0);
      ShowScreenInSystemContents(GURL(), /*show_toolbar=*/false);
      // Reset the sign-in flow.
      sign_in_.reset();
      toolbar_->RemoveAllChildViews(/*delete_children=*/true);
      return;
    }
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile: {
      // Finished here, avoid aborting the flow in the destructor (which is
      // called as a result of Clear()).
      sign_in_->Cancel();
      Clear();
      return;
    }
  }
}

void ProfilePickerView::OnProfileForSigninCreated(
    SkColor profile_color,
    base::OnceCallback<void(bool)>* switch_finished_callback,
    Profile* profile,
    Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_LOCAL_FAIL) {
    std::move(*switch_finished_callback).Run(false);
    return;
  } else if (status != Profile::CREATE_STATUS_INITIALIZED) {
    return;
  }

  DCHECK(profile);
  std::move(*switch_finished_callback).Run(true);

  if (signin_util::IsForceSigninEnabled()) {
    // Show the embedded sign-in flow if the force signin is enabled.
    ProfilePickerForceSigninDialog::ShowForceSigninDialog(
        web_view_->GetWebContents()->GetBrowserContext(), profile->GetPath());
    return;
  }

  sign_in_ = std::make_unique<ProfilePickerSignInFlowController>(
      this, profile, profile_color, extended_account_info_timeout_);
  sign_in_->Init();
}

void ProfilePickerView::WindowClosing() {
  // Now that the window is closed, we can allow a new one to be opened.
  // (WindowClosing comes in asynchronously from the call to Close() and we
  // may have already opened a new instance).
  if (g_profile_picker_view == this)
    g_profile_picker_view = nullptr;

  // Show a new profile window if it has been requested while the current window
  // was closing.
  if (state_ == kClosing && restart_with_entry_point_on_window_closing_) {
    ProfilePicker::Show(*restart_with_entry_point_on_window_closing_);
  }
}

views::ClientView* ProfilePickerView::CreateClientView(views::Widget* widget) {
  return new views::ClientView(widget, TransferOwnershipOfContentsView());
}

views::View* ProfilePickerView::GetContentsView() {
  return this;
}

std::u16string ProfilePickerView::GetAccessibleWindowTitle() const {
  if (!web_view_ || !web_view_->GetWebContents() ||
      web_view_->GetWebContents()->GetTitle().empty()) {
    return l10n_util::GetStringUTF16(IDS_PROFILE_PICKER_MAIN_VIEW_TITLE);
  }
  return web_view_->GetWebContents()->GetTitle();
}

gfx::Size ProfilePickerView::CalculatePreferredSize() const {
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
  // Ignore presses of the Escape key. The profile picker may be Chrome's only
  // top-level window, in which case we don't want presses of Esc to maybe quit
  // the entire browser. This has higher priority than the default dialog Esc
  // accelerator (which would otherwise close the window).
  if (accelerator.key_code() == ui::VKEY_ESCAPE &&
      accelerator.modifiers() == ui::EF_NONE) {
    return true;
  }

  const auto& iter = accelerator_table_.find(accelerator);
  DCHECK(iter != accelerator_table_.end());
  int command_id = iter->second;
  switch (command_id) {
    case IDC_CLOSE_TAB:
    case IDC_CLOSE_WINDOW:
      // kEscKeyPressed is used although that shortcut is disabled (this is
      // Ctrl-Shift-W instead).
      GetWidget()->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
      break;
    case IDC_EXIT:
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
    // Always reload bypassing cache.
    case IDC_RELOAD:
    case IDC_RELOAD_BYPASSING_CACHE:
    case IDC_RELOAD_CLEARING_CACHE: {
      // Sign-in may fail due to connectivity issues, allow reloading.
      if (GetSigningIn()) {
        sign_in_->contents()->GetController().Reload(
            content::ReloadType::BYPASSING_CACHE, true);
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected command_id: " << command_id;
      break;
  }

  return true;
}

void ProfilePickerView::OnThemeChanged() {
  views::WidgetDelegateView::OnThemeChanged();
  if (!GetSigningIn())
    return;
  UpdateToolbarColor();
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

  auto toolbar = std::make_unique<views::View>();
  toolbar->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true)
      .SetInteriorMargin(kToolbarPadding);
  toolbar->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));
  // It is important for tests that the toolbar starts visible (being empty).
  toolbar_ = AddChildView(std::move(toolbar));

  auto web_view = std::make_unique<views::WebView>();
  web_view->set_allow_accelerators(true);
  web_view_ = AddChildView(std::move(web_view));
}

void ProfilePickerView::UpdateToolbarColor() {
  // The sign-in profile is needed to obtain the ThemeProvider.
  DCHECK(sign_in_->profile());
  SkColor background_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR);
  toolbar_->SetBackground(views::CreateSolidBackground(background_color));

  // On Mac, the WebContents is initially transparent. Set the color for the
  // main view as well.
  SetBackground(views::CreateSolidBackground(background_color));
}

void ProfilePickerView::ShowScreenFinished(
    content::WebContents* contents,
    bool show_toolbar,
    bool enable_navigating_back,
    base::OnceClosure navigation_finished_closure) {
  // Stop observing for this (or any previous) navigation.
  if (show_screen_finished_observer_)
    show_screen_finished_observer_.reset();

  web_view_->SetWebContents(contents);
  contents->Focus();

  // Change visibility of the toolbar after swapping wc in `web_view_` to make
  // it easier for tests to detect changing of the screen.
  toolbar_->SetVisible(show_toolbar);
  enable_navigating_back_ = enable_navigating_back;

  if (navigation_finished_closure)
    std::move(navigation_finished_closure).Run();
}

void ProfilePickerView::BackButtonPressed(const ui::Event& event) {
  NavigateBack();
}

void ProfilePickerView::NavigateBack() {
  if (!enable_navigating_back_)
    return;

  if (web_view_->GetWebContents()->GetController().CanGoBack()) {
    web_view_->GetWebContents()->GetController().GoBack();
    return;
  }

  // Move from sign-in back to the previous screen of profile creation.
  // Do not load any url because the desired screen is still loaded in
  // `system_profile_contents_`.
  if (GetSigningIn())
    ShowScreenInSystemContents(GURL(), /*show_toolbar=*/false);
}

bool ProfilePickerView::GetSigningIn() const {
  // We are in the sign-in flow if the sign-in contents is displayed and is used
  // for signing in.
  return sign_in_ && sign_in_->IsSigningIn();
}

void ProfilePickerView::SetExtendedAccountInfoTimeoutForTesting(
    base::TimeDelta timeout) {
  extended_account_info_timeout_ = timeout;
}

void ProfilePickerView::ConfigureAccelerators() {
  // By default, dialog views close when pressing escape. Override this
  // behavior as the profile picker should not close in that case.
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  const std::vector<AcceleratorMapping> accelerator_list(GetAcceleratorList());
  for (const auto& entry : accelerator_list) {
    if (!base::Contains(kSupportedAcceleratorCommands, entry.command_id))
      continue;
    ui::Accelerator accelerator(entry.keycode, entry.modifiers);
    accelerator_table_[accelerator] = entry.command_id;
    AddAccelerator(accelerator);
  }

#if defined(OS_MAC)
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
#endif  // OS_MAC
}

void ProfilePickerView::ShowDialog(content::BrowserContext* browser_context,
                                   const GURL& url,
                                   const base::FilePath& profile_path) {
  gfx::NativeView parent = GetWidget()->GetNativeView();
  dialog_host_.ShowDialog(browser_context, url, profile_path, parent);
}

void ProfilePickerView::HideDialog() {
  dialog_host_.HideDialog();
}

base::FilePath ProfilePickerView::GetForceSigninProfilePath() const {
  return dialog_host_.GetForceSigninProfilePath();
}

GURL ProfilePickerView::GetOnSelectProfileTargetUrl() const {
  return on_select_profile_target_url_;
}

BEGIN_METADATA(ProfilePickerView, views::WidgetDelegateView)
ADD_READONLY_PROPERTY_METADATA(bool, SigningIn)
ADD_READONLY_PROPERTY_METADATA(base::FilePath, ForceSigninProfilePath)
END_METADATA
