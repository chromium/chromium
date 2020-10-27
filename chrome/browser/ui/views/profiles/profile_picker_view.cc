// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_sync_delegate.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "chrome/browser/ui/webui/signin/signin_web_dialog_ui.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/url_constants.h"

#if defined(OS_WIN)
#include "chrome/browser/shell_integration_win.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace {

ProfilePickerView* g_profile_picker_view = nullptr;
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 758;
constexpr float kMaxRatioOfWorkArea = 0.9;

constexpr base::TimeDelta kExtendedAccountInfoTimeout =
    base::TimeDelta::FromSeconds(10);

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
      return base_url;
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile:
      return base_url.Resolve("new-profile");
  }
}

}  // namespace

// static
void ProfilePicker::Show(EntryPoint entry_point) {
  if (!g_profile_picker_view)
    g_profile_picker_view = new ProfilePickerView();

  g_profile_picker_view->Display(entry_point);
}

// static
void ProfilePicker::SwitchToSignIn(SkColor profile_color,
                                   base::OnceClosure switch_failure_callback) {
  if (g_profile_picker_view) {
    g_profile_picker_view->SwitchToSignIn(profile_color,
                                          std::move(switch_failure_callback));
  }
}

// static
void ProfilePicker::SwitchToSyncConfirmation() {
  if (g_profile_picker_view) {
    g_profile_picker_view->SwitchToSyncConfirmation();
  }
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
void ProfilePicker::SetExtendedAccountInfoTimeoutForTesting(
    base::TimeDelta timeout) {
  if (g_profile_picker_view) {
    g_profile_picker_view->SetExtendedAccountInfoTimeoutForTesting(  // IN-TEST
        timeout);
  }
}

ProfilePickerView::ProfilePickerView()
    : keep_alive_(KeepAliveOrigin::USER_MANAGER_VIEW,
                  KeepAliveRestartOption::DISABLED),
      extended_account_info_timeout_(kExtendedAccountInfoTimeout) {
  SetHasWindowSizeControls(true);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetTitle(IDS_PRODUCT_NAME);
  set_use_custom_frame(false);
  // TODO(crbug.com/1063856): Add |RecordDialogCreation|.
}

ProfilePickerView::~ProfilePickerView() = default;

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
    g_browser_process->profile_manager()->CreateProfileAsync(
        ProfileManager::GetSystemProfilePath(),
        base::BindRepeating(&ProfilePickerView::OnSystemProfileCreated,
                            weak_ptr_factory_.GetWeakPtr(), entry_point),
        /*name=*/base::string16(), /*icon_url=*/std::string());
    return;
  }

  if (state_ == kInitializing)
    return;

  GetWidget()->Activate();
}

void ProfilePickerView::Clear() {
  if (state_ == kReady || state_ == kFinalizing) {
    GetWidget()->Close();
    return;
  }

  WindowClosing();
  DeleteDelegate();
}

void ProfilePickerView::OnSystemProfileCreated(
    ProfilePicker::EntryPoint entry_point,
    Profile* system_profile,
    Profile::CreateStatus status) {
  DCHECK_NE(status, Profile::CREATE_STATUS_LOCAL_FAIL);
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  Init(entry_point, system_profile);
}

void ProfilePickerView::Init(ProfilePicker::EntryPoint entry_point,
                             Profile* system_profile) {
  DCHECK_EQ(state_, kInitializing);
  auto web_view = std::make_unique<views::WebView>(system_profile);
  web_view->GetWebContents()->SetDelegate(this);
  // To record metrics using javascript, extensions are needed.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_view->GetWebContents());
  // Set the member before adding to the hieararchy to make it easier for tests
  // to detect that a new WebView has been created.
  web_view_ = web_view.get();
  AddChildView(std::move(web_view));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  CreateDialogWidget(this, nullptr, nullptr);

#if defined(OS_WIN)
  // Set the app id for the user manager to the app id of its parent.
  ui::win::SetAppIdForWindow(
      shell_integration::win::GetAppUserModelIdForBrowser(
          system_profile->GetPath()),
      views::HWNDForWidget(GetWidget()));
#endif

  web_view_->LoadInitialURL(CreateURLForEntryPoint(entry_point));
  GetWidget()->Show();
  web_view_->RequestFocus();
  state_ = kReady;

  if (entry_point == ProfilePicker::EntryPoint::kOnStartup) {
    DCHECK(!creation_time_on_startup_.is_null());
    base::UmaHistogramTimes("ProfilePicker.StartupTime.WebViewCreated",
                            base::TimeTicks::Now() - creation_time_on_startup_);
  }
}

void ProfilePickerView::SwitchToSignIn(
    SkColor profile_color,
    base::OnceClosure switch_failure_callback) {
  DCHECK(!switch_failure_callback_);
  switch_failure_callback_ = std::move(switch_failure_callback);
  size_t icon_index = profiles::GetPlaceholderAvatarIndex();
  // Silently create the new profile for browsing on GAIA (so that the sign-in
  // cookies are stored in the right profile).
  ProfileManager::CreateMultiProfileAsync(
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .ChooseNameForNewProfile(icon_index),
      profiles::GetDefaultAvatarIconUrl(icon_index),
      base::BindRepeating(&ProfilePickerView::OnProfileForSigninCreated,
                          weak_ptr_factory_.GetWeakPtr(), profile_color));
}

void ProfilePickerView::OnProfileForSigninCreated(
    SkColor profile_color,
    Profile* profile,
    Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_LOCAL_FAIL) {
    if (switch_failure_callback_)
      std::move(switch_failure_callback_).Run();
    return;
  } else if (status != Profile::CREATE_STATUS_INITIALIZED) {
    return;
  }

  // No need to report failure any more, delete the callback.
  DCHECK(switch_failure_callback_);
  switch_failure_callback_ = base::OnceClosure();

  DCHECK(profile);

  ProfileAttributesEntry* entry = nullptr;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    NOTREACHED();
    return;
  }

  // Mark this profile ephemeral so that it is deleted upon next startup if the
  // browser crashes before finishing the flow.
  entry->SetIsEphemeral(true);

  // Apply a new color to the profile.
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile);
  theme_service->BuildAutogeneratedThemeFromColor(profile_color);

  // TODO(crbug.com/1126913): Record also that we show the sign-in promo
  // (it has to be plumbed from js to profile_picker_handler.cc):
  //   signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
  //       signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER);

  // Record that the sign in process starts (its end is recorded automatically
  // by the instance of DiceTurnSyncOnHelper constructed later on).
  signin_metrics::RecordSigninUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
  signin_metrics::LogSigninAccessPointStarted(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);

  // Listen for sign-in getting completed.
  identity_manager_observer_.Add(
      IdentityManagerFactory::GetForProfile(profile));
  // TODO(crbug.com/1126913): When there is back button from the signed-in page,
  // make sure the flow does not create multiple profiles simultaneously.
  signed_in_profile_being_created_ = profile;

  // Rebuild the view.
  // TODO(crbug.com/1126913): Add the simple toolbar with the back button.
  RemoveAllChildViews(true);
  auto web_view = std::make_unique<views::WebView>(profile);
  web_view->GetWebContents()->SetDelegate(this);
  // Set the member before adding to the hieararchy to make it easier for tests
  // to detect that a new WebView has been created.
  web_view_ = web_view.get();
  AddChildView(std::move(web_view));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  web_view_->LoadInitialURL(GaiaUrls::GetInstance()->signin_chrome_sync_dice());
  web_view_->RequestFocus();
}

void ProfilePickerView::SwitchToSyncConfirmation() {
  // TODO(crbug.com/1126913): Remove the simple toolbar with the back button
  // (once it is added for the GAIA part).
  web_view_->LoadInitialURL(GURL(chrome::kChromeUISyncConfirmationURL));
  web_view_->RequestFocus();

  SyncConfirmationUI* sync_confirmation_ui = static_cast<SyncConfirmationUI*>(
      web_view_->GetWebContents()->GetWebUI()->GetController());
  sync_confirmation_ui->InitializeMessageHandlerWithProfile(
      signed_in_profile_being_created_);
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

void ProfilePickerView::WindowClosing() {
  // Now that the window is closed, we can allow a new one to be opened.
  // (WindowClosing comes in asynchronously from the call to Close() and we
  // may have already opened a new instance).
  if (g_profile_picker_view == this)
    g_profile_picker_view = nullptr;
}

gfx::Size ProfilePickerView::GetMinimumSize() const {
  // On small screens, the preferred size may be smaller than the picker
  // minimum size. In that case there will be scrollbars on the picker.
  gfx::Size minimum_size = GetPreferredSize();
  minimum_size.SetToMin(ProfilePickerUI::GetMinimumSize());
  return minimum_size;
}

bool ProfilePickerView::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

void ProfilePickerView::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  DCHECK(signed_in_profile_being_created_)
      << "Opening new tabs should only happen within GAIA signin";
  NavigateParams params(signed_in_profile_being_created_, target_url,
                        ui::PAGE_TRANSITION_LINK);
  // Open all links as new popups.
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.contents_to_insert = std::move(new_contents);
  params.window_bounds = initial_rect;
  Navigate(&params);
}

void ProfilePickerView::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  DCHECK(!account_info.IsEmpty());

  base::OnceClosure sync_consent_completed_closure =
      base::BindOnce(&ProfilePickerView::FinishSignedInCreationFlow,
                     weak_ptr_factory_.GetWeakPtr(), BrowserOpenedCallback());

  // Stop with the sign-in navigation, it is not needed any more and this avoids
  // any glitches of the redirect page getting displayed. This is needed because
  // in some cases (such as managed signed-in), there are further delays before
  // any follow-up UI is shown.
  web_view_->LoadInitialURL(GURL(url::kAboutBlankURL));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ProfilePickerView::OnExtendedAccountInfoTimeout,
                     weak_ptr_factory_.GetWeakPtr(), account_info.email),
      extended_account_info_timeout_);

  // DiceTurnSyncOnHelper deletes itself once done.
  new DiceTurnSyncOnHelper(
      signed_in_profile_being_created_,
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT,
      account_info.account_id,
      DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
      std::make_unique<ProfilePickerViewSyncDelegate>(
          signed_in_profile_being_created_,
          base::BindOnce(&ProfilePickerView::FinishSignedInCreationFlow,
                         weak_ptr_factory_.GetWeakPtr())),
      std::move(sync_consent_completed_closure));
}

void ProfilePickerView::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  if (!account_info.IsValid())
    return;
  name_for_signed_in_profile_ =
      profiles::GetDefaultNameForNewSignedInProfile(account_info);
  OnProfileNameAvailable();
}

void ProfilePickerView::SetExtendedAccountInfoTimeoutForTesting(
    base::TimeDelta timeout) {
  extended_account_info_timeout_ = timeout;
}

void ProfilePickerView::OnExtendedAccountInfoTimeout(const std::string& email) {
  // As a fallback, use the email of the user as the profile name when extended
  // account info is not available.
  name_for_signed_in_profile_ = base::UTF8ToUTF16(email);
  OnProfileNameAvailable();
}

void ProfilePickerView::OnProfileNameAvailable() {
  // Stop listening to further changes.
  identity_manager_observer_.Remove(
      IdentityManagerFactory::GetForProfile(signed_in_profile_being_created_));

  if (on_profile_name_available_)
    std::move(on_profile_name_available_).Run();
}

void ProfilePickerView::FinishSignedInCreationFlow(
    BrowserOpenedCallback callback) {
  // This can get called first time from a special case handling (such as the
  // Settings link) and than second time when the consent flow finishes. We need
  // to make sure only the first call gets handled.
  if (state_ == kFinalizing)
    return;
  state_ = kFinalizing;

  if (name_for_signed_in_profile_.empty()) {
    on_profile_name_available_ =
        base::BindOnce(&ProfilePickerView::FinishSignedInCreationFlowImpl,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback));
    return;
  }

  FinishSignedInCreationFlowImpl(std::move(callback));
}

void ProfilePickerView::FinishSignedInCreationFlowImpl(
    BrowserOpenedCallback callback) {
  DCHECK(!name_for_signed_in_profile_.empty());

  ProfileAttributesEntry* entry = nullptr;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(
               signed_in_profile_being_created_->GetPath(), &entry)) {
    NOTREACHED();
    return;
  }

  // Unmark this profile ephemeral so that it is not deleted upon next startup.
  entry->SetIsEphemeral(false);
  entry->SetLocalProfileName(name_for_signed_in_profile_);

  // TODO(crbug.com/1126913): Change the callback of
  // profiles::OpenBrowserWindowForProfile() to be a OnceCallback as it is only
  // called once.
  profiles::OpenBrowserWindowForProfile(
      base::AdaptCallbackForRepeating(
          base::BindOnce(&ProfilePickerView::OnBrowserOpened,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))),
      /*always_create=*/false,   // Don't create a window if one already exists.
      /*is_new_profile=*/false,  // Don't create a first run window.
      /*unblock_extensions=*/false,  // There is no need to unblock all
                                     // extensions because we only open browser
                                     // window if the Profile is not locked.
                                     // Hence there is no extension blocked.
      signed_in_profile_being_created_, Profile::CREATE_STATUS_INITIALIZED);
}

void ProfilePickerView::OnBrowserOpened(
    BrowserOpenedCallback finish_flow_callback,
    Profile* profile,
    Profile::CreateStatus profile_create_status) {
  DCHECK_EQ(profile, signed_in_profile_being_created_);

  // Hide the flow window. This posts a task on the message loop to destroy the
  // window incl. this view.
  Clear();

  if (!finish_flow_callback)
    return;

  Browser* browser =
      chrome::FindLastActiveWithProfile(signed_in_profile_being_created_);
  DCHECK(browser);
  std::move(finish_flow_callback).Run(browser);
}
