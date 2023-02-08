// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_flow_controller.h"

#include <string>

#include "base/trace_event/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_utils.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_metrics.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/views/profiles/profile_picker_dice_sign_in_provider.h"
#endif

namespace {

const signin_metrics::AccessPoint kAccessPoint =
    signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER;

// Returns the URL to load as initial content for the profile picker. If an
// empty URL is returned, the profile picker should not be shown until
// another explicit call with a non-empty URL given to the view
// (see `ProfilePickerView::ShowScreen()` for example)
GURL GetInitialURL(ProfilePicker::EntryPoint entry_point) {
  GURL base_url = GURL(chrome::kChromeUIProfilePickerUrl);
  switch (entry_point) {
    case ProfilePicker::EntryPoint::kOnStartup:
    case ProfilePicker::EntryPoint::kOnStartupNoProfile: {
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
    case ProfilePicker::EntryPoint::kProfileIdle:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcessNoProfile:
      return base_url;
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile:
      return base_url.Resolve("new-profile");
    case ProfilePicker::EntryPoint::kLacrosSelectAvailableAccount:
      return base_url.Resolve("account-selection-lacros");
    case ProfilePicker::EntryPoint::kLacrosPrimaryProfileFirstRun:
    case ProfilePicker::EntryPoint::kFirstRun:
      // Should not be used for this entry point.
      NOTREACHED();
      return GURL();
  }
}

// Shows the customization bubble if possible. The bubble won't be shown if the
// color is enforced by policy or downloaded through Sync or the default theme
// should be used. An IPH is shown after the bubble, or right away if the bubble
// cannot be shown.
void ShowCustomizationBubble(absl::optional<SkColor> new_profile_color,
                             Browser* browser) {
  DCHECK(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->toolbar_button_provider())
    return;
  views::View* anchor_view =
      browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
  CHECK(anchor_view);

  if (ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
          browser->profile())) {
    // For sync users, their profile color has not been applied yet. Call a
    // helper class that applies the color and shows the bubble only if there is
    // no conflict with a synced theme / color.
    ProfileCustomizationBubbleSyncController::
        ApplyColorAndShowBubbleWhenNoValueSynced(
            browser, anchor_view,
            /*suggested_profile_color=*/new_profile_color.value());
  } else {
    // For non syncing users, simply show the bubble.
    ProfileCustomizationBubbleView::CreateBubble(browser, anchor_view);
  }
}

void MaybeShowProfileSwitchIPH(Browser* browser) {
  DCHECK(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view)
    return;
  browser_view->MaybeShowProfileSwitchIPH();
}

// Class triggering the signed-in section of the profile management flow, most
// notably featuring the sync confirmation. In addition to what its base class
// `ProfilePickerSignedInFlowController` is doing, this class:
// - shows in product help and customization bubble at the end of the flow
// - applies profile customizations (theme, profile name)
// - finalizes the profile (deleting it if the flow is aborted, marks it
//   non-ephemeral if the flow is completed)
// `finish_flow_callback` is not called if the flow is canceled.
class ProfileCreationSignedInFlowController
    : public ProfilePickerSignedInFlowController {
 public:
  ProfileCreationSignedInFlowController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      std::unique_ptr<content::WebContents> contents,
      absl::optional<SkColor> profile_color,
      FinishFlowCallback finish_flow_callback)
      : ProfilePickerSignedInFlowController(host,
                                            profile,
                                            std::move(contents),
                                            kAccessPoint,
                                            profile_color),
        finish_flow_callback_(std::move(finish_flow_callback)) {}

  ProfileCreationSignedInFlowController(
      const ProfilePickerSignedInFlowController&) = delete;
  ProfileCreationSignedInFlowController& operator=(
      const ProfilePickerSignedInFlowController&) = delete;

  ~ProfileCreationSignedInFlowController() override {
    // Record unfinished signed-in profile creation.
    if (!is_finishing_) {
      // Schedule the profile for deletion, it's not needed any more.
      g_browser_process->profile_manager()
          ->GetDeleteProfileHelper()
          .ScheduleEphemeralProfileForDeletion(profile()->GetPath());

      // TODO(crbug.com/1300109): Consider moving this recording into
      // ProfilePickerTurnSyncOnDelegate and unify this code with Cancel().
      ProfileMetrics::LogProfileAddSignInFlowOutcome(
          ProfileMetrics::ProfileSignedInFlowOutcome::kAbortedAfterSignIn);
    }
  }

  // ProfilePickerSignedInFlowController:
  void Init() override {
    // Stop with the sign-in navigation and show a spinner instead. The spinner
    // will be shown until TurnSyncOnHelper figures out whether it's a
    // managed account and whether sync is disabled by policies (which in some
    // cases involves fetching policies and can take a couple of seconds).
    host()->ShowScreen(contents(), GetSyncConfirmationURL(/*loading=*/true));

    ProfilePickerSignedInFlowController::Init();

    // Listen for extended account info getting fetched.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    profile_name_resolver_ =
        std::make_unique<ProfileNameResolver>(identity_manager);
  }

  void Cancel() override {
    if (is_finishing_)
      return;

    is_finishing_ = true;

    // Schedule the profile for deletion, it's not needed any more.
    g_browser_process->profile_manager()
        ->GetDeleteProfileHelper()
        .ScheduleEphemeralProfileForDeletion(profile()->GetPath());
  }

  void FinishAndOpenBrowser(PostHostClearedCallback callback) override {
    // Do nothing if the sign-in flow is aborted or if this has already been
    // called. Note that this can get called first time from a special case
    // handling (such as the Settings link) and than second time when the
    // TurnSyncOnHelper finishes.
    if (is_finishing_)
      return;
    is_finishing_ = true;

    if (callback->is_null()) {
      // No custom callback is specified, we can schedule a profile-related
      // experience to be shown in context of the opened fresh profile.
      callback = CreateFreshProfileExperienceCallback();
    }
    DCHECK(callback.value());

    if (profile_name_resolver_->resolved_profile_name().empty()) {
      // Delay finishing the flow until we have obtained a profile name.
      profile_name_resolver_->set_on_profile_name_resolved_callback(
          base::BindOnce(
              &ProfileCreationSignedInFlowController::FinishFlow,
              // Unretained ok: `this` outlives `profile_name_resolver_`.
              base::Unretained(this), std::move(callback)));
    } else {
      FinishFlow(std::move(callback));
    }
  }

 private:
  PostHostClearedCallback CreateFreshProfileExperienceCallback() {
    // If there's no color to apply to the profile, skip the customization
    // bubble and trigger an IPH, instead.
    if (ThemeServiceFactory::GetForProfile(profile())->UsingPolicyTheme() ||
        !GetProfileColor().has_value()) {
      return PostHostClearedCallback(
          base::BindOnce(&MaybeShowProfileSwitchIPH));
    } else {
      // If sync cannot start, we apply `GetProfileColor()` right away before
      // opening a browser window to avoid flicker. Otherwise, it's applied
      // later by code triggered from ShowCustomizationBubble().
      if (!ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
              profile())) {
        auto* theme_service = ThemeServiceFactory::GetForProfile(profile());
        theme_service->BuildAutogeneratedThemeFromColor(
            GetProfileColor().value());
      }
      return PostHostClearedCallback(
          base::BindOnce(&ShowCustomizationBubble, GetProfileColor()));
    }
  }

  void FinishFlow(PostHostClearedCallback callback) {
    TRACE_EVENT1("browser", "ProfileCreationSignedInFlowController::FinishFlow",
                 "profile_path", profile()->GetPath().AsUTF8Unsafe());
    std::u16string name_for_signed_in_profile =
        profile_name_resolver_->resolved_profile_name();
    profile_name_resolver_.reset();
    DCHECK(!name_for_signed_in_profile.empty());
    DCHECK(callback.value());
    DCHECK(finish_flow_callback_.value());

    FinalizeNewProfileSetup(profile(), name_for_signed_in_profile);

    ProfileMetrics::LogProfileAddNewUser(
        ProfileMetrics::ADD_NEW_PROFILE_PICKER_SIGNED_IN);

    std::move(finish_flow_callback_.value()).Run(std::move(callback));
  }

  // Controls whether the flow still needs to finalize (which includes showing
  // `profile` browser window at the end of the sign-in flow).
  bool is_finishing_ = false;

  std::unique_ptr<ProfileNameResolver> profile_name_resolver_;
  FinishFlowCallback finish_flow_callback_;
};

}  // namespace

ProfilePickerFlowController::ProfilePickerFlowController(
    ProfilePickerWebContentsHost* host,
    ClearHostClosure clear_host_callback,
    ProfilePicker::EntryPoint entry_point)
    : ProfileManagementFlowControllerImpl(host, std::move(clear_host_callback)),
      entry_point_(entry_point) {}

ProfilePickerFlowController::~ProfilePickerFlowController() = default;

void ProfilePickerFlowController::Init(
    StepSwitchFinishedCallback step_switch_finished_callback) {
  RegisterStep(Step::kProfilePicker,
               ProfileManagementStepController::CreateForProfilePickerApp(
                   host(), GetInitialURL(entry_point_)));
  SwitchToStep(Step::kProfilePicker, /*reset_state=*/true,
               std::move(step_switch_finished_callback));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProfilePickerFlowController::SwitchToDiceSignIn(
    absl::optional<SkColor> profile_color,
    StepSwitchFinishedCallback switch_finished_callback) {
  DCHECK_EQ(Step::kProfilePicker, current_step());

  profile_color_ = profile_color;
  SwitchToIdentityStepsFromAccountSelection(
      std::move(switch_finished_callback));
}
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ProfilePickerFlowController::SwitchToPostSignIn(
    Profile* signed_in_profile,
    absl::optional<SkColor> profile_color,
    std::unique_ptr<content::WebContents> contents) {
  DCHECK_EQ(Step::kProfilePicker, current_step());
  profile_color_ = profile_color;
  SwitchToIdentityStepsFromPostSignIn(
      signed_in_profile,
      content::WebContents::Create(
          content::WebContents::CreateParams(signed_in_profile)),
      StepSwitchFinishedCallback());
}
#endif

base::FilePath ProfilePickerFlowController::GetSwitchProfilePathOrEmpty()
    const {
  if (weak_signed_in_flow_controller_) {
    return weak_signed_in_flow_controller_->switch_profile_path();
  }
  return base::FilePath();
}

void ProfilePickerFlowController::CancelPostSignInFlow() {
  // Triggered from either entreprise welcome or profile switch screens.
  DCHECK_EQ(Step::kPostSignInFlow, current_step());

  switch (entry_point_) {
    case ProfilePicker::EntryPoint::kOnStartup:
    case ProfilePicker::EntryPoint::kOnStartupNoProfile:
    case ProfilePicker::EntryPoint::kProfileMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcess:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcessNoProfile:
    case ProfilePicker::EntryPoint::kProfileLocked:
    case ProfilePicker::EntryPoint::kUnableToCreateBrowser:
    case ProfilePicker::EntryPoint::kBackgroundModeManager:
    case ProfilePicker::EntryPoint::kProfileIdle: {
      SwitchToStep(Step::kProfilePicker, /*reset_state=*/true);
      UnregisterStep(Step::kPostSignInFlow);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      UnregisterStep(Step::kAccountSelection);
#endif
      return;
    }
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile: {
      // This results in destroying `this`.
      ExitFlow();
      return;
    }
    case ProfilePicker::EntryPoint::kLacrosSelectAvailableAccount:
    case ProfilePicker::EntryPoint::kLacrosPrimaryProfileFirstRun:
    case ProfilePicker::EntryPoint::kFirstRun:
      NOTREACHED()
          << "CancelPostSignInFlow() is not reachable from this entry point";
      return;
  }
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
std::unique_ptr<ProfilePickerDiceSignInProvider>
ProfilePickerFlowController::CreateDiceSignInProvider() {
  return std::make_unique<ProfilePickerDiceSignInProvider>(host(),
                                                           kAccessPoint);
}

absl::optional<SkColor> ProfilePickerFlowController::GetProfileColor() {
  return profile_color_;
}
#endif

std::unique_ptr<ProfilePickerSignedInFlowController>
ProfilePickerFlowController::CreateSignedInFlowController(
    Profile* signed_in_profile,
    std::unique_ptr<content::WebContents> contents,
    FinishFlowCallback finish_flow_callback) {
  DCHECK(!weak_signed_in_flow_controller_);
  auto signed_in_flow = std::make_unique<ProfileCreationSignedInFlowController>(
      host(), signed_in_profile, std::move(contents), profile_color_,
      std::move(finish_flow_callback));
  weak_signed_in_flow_controller_ = signed_in_flow->GetWeakPtr();
  return signed_in_flow;
}
