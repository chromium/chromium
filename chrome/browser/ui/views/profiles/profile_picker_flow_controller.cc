// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_flow_controller.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/not_fatal_until.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/profiles/profile_customization_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_dice_reauth_provider.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/core_account_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/themes.mojom.h"

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
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuManageProfiles:
      return base_url;
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile:
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuAddNewProfile:
      return base_url.Resolve("new-profile");
    case ProfilePicker::EntryPoint::kLacrosSelectAvailableAccount:
      return base_url.Resolve("account-selection-lacros");
    case ProfilePicker::EntryPoint::kLacrosPrimaryProfileFirstRun:
    case ProfilePicker::EntryPoint::kFirstRun:
      // Should not be used for this entry point.
      NOTREACHED();
  }
}

// Shows the customization bubble if possible. The bubble won't be shown if the
// color is enforced by policy or downloaded through Sync or the default theme
// should be used. An IPH is shown after the bubble, or right away if the bubble
// cannot be shown.
void ShowCustomizationBubble(std::optional<SkColor> new_profile_color,
                             Browser* browser) {
  DCHECK(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->toolbar_button_provider()) {
    return;
  }
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
  if (!browser_view) {
    return;
  }
  browser_view->MaybeShowProfileSwitchIPH();
}

// Class triggering the signed-in section of the profile management flow, most
// notably featuring the sync confirmation. In addition to what its base class
// `ProfilePickerSignedInFlowController` is doing, this class:
// - shows in product help and customization bubble at the end of the flow
// - applies profile customizations (theme, profile name)
// - finalizes the profile (deleting it if the flow is aborted, marks it
//   non-ephemeral if the flow is completed)
// `step_completed_callback` is not called if the flow is canceled.
// Note that `account_id` has been added to the `IdentityManager` but may not
// be set as primary yet, because this operation is asynchronous.
class ProfileCreationSignedInFlowController
    : public ProfilePickerSignedInFlowController {
 public:
  ProfileCreationSignedInFlowController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents,
      std::optional<SkColor> profile_color,
      base::OnceCallback<
          void(PostHostClearedCallback, bool, StepSwitchFinishedCallback)>
          step_completed_callback)
      : ProfilePickerSignedInFlowController(host,
                                            profile,
                                            account_info,
                                            std::move(contents),
                                            kAccessPoint,
                                            profile_color),
        step_completed_callback_(std::move(step_completed_callback)) {}

  ProfileCreationSignedInFlowController(
      const ProfilePickerSignedInFlowController&) = delete;
  ProfileCreationSignedInFlowController& operator=(
      const ProfilePickerSignedInFlowController&) = delete;

  ~ProfileCreationSignedInFlowController() override {
    // Record unfinished signed-in profile creation.
    if (!is_finishing_) {
      // TODO(crbug.com/40216113): Consider moving this recording into
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
        std::make_unique<ProfileNameResolver>(identity_manager, account_info());
  }

  void Cancel() override {
    if (is_finishing_) {
      return;
    }

    is_finishing_ = true;
  }

  void FinishAndOpenBrowserInternal(PostHostClearedCallback callback,
                                    bool is_continue_callback) override {
    // Do nothing if the sign-in flow is aborted or if this has already been
    // called. Note that this can get called first time from a special case
    // handling (such as the Settings link) and than second time when the
    // TurnSyncOnHelper finishes.
    if (is_finishing_) {
      return;
    }
    is_finishing_ = true;
    callback =
        callback->is_null()
            ? CreateFreshProfileExperienceCallback()
            : PostHostClearedCallback(base::BindOnce(
                  [](PostHostClearedCallback cb1, PostHostClearedCallback cb2,
                     Browser* browser) {
                    std::move(*cb1).Run(browser);
                    std::move(*cb2).Run(browser);
                  },
                  std::move(callback), CreateFreshProfileExperienceCallback()));

    profile_name_resolver_->RunWithProfileName(base::BindOnce(
        &ProfileCreationSignedInFlowController::FinishFlow,
        // Unretained ok: `this` outlives `profile_name_resolver_`.
        base::Unretained(this), std::move(callback), is_continue_callback));
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
        theme_service->SetUserColorAndBrowserColorVariant(
            GetProfileColor().value(),
            ui::mojom::BrowserColorVariant::kTonalSpot);
      }
      return PostHostClearedCallback(
          base::BindOnce(&ShowCustomizationBubble, GetProfileColor()));
    }
  }

  void FinishFlow(PostHostClearedCallback post_host_cleared_callback,
                  bool is_continue_callback,
                  std::u16string name_for_signed_in_profile) {
    TRACE_EVENT1("browser", "ProfileCreationSignedInFlowController::FinishFlow",
                 "profile_path", profile()->GetPath().AsUTF8Unsafe());
    CHECK(!name_for_signed_in_profile.empty());
    DCHECK(post_host_cleared_callback.value());
    DCHECK(step_completed_callback_);

    profile_name_resolver_.reset();

    FinalizeNewProfileSetup(profile(), name_for_signed_in_profile,
                            /*is_default_name=*/false);

    ProfileMetrics::LogProfileAddNewUser(
        ProfileMetrics::ADD_NEW_PROFILE_PICKER_SIGNED_IN);

    std::move(step_completed_callback_)
        .Run(std::move(post_host_cleared_callback), is_continue_callback,
             StepSwitchFinishedCallback());
  }

  // Controls whether the flow still needs to finalize (which includes showing
  // `profile` browser window at the end of the sign-in flow).
  bool is_finishing_ = false;

  std::unique_ptr<ProfileNameResolver> profile_name_resolver_;
  base::OnceCallback<
      void(PostHostClearedCallback, bool, StepSwitchFinishedCallback)>
      step_completed_callback_;
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ReauthFlowStepController : public ProfileManagementStepController {
 public:
  explicit ReauthFlowStepController(
      ProfilePickerWebContentsHost* host,
      std::unique_ptr<ProfilePickerDiceReauthProvider> reauth_provider,
      Profile* profile)
      : ProfileManagementStepController(host),
        reauth_provider_(std::move(reauth_provider)) {}

  ~ReauthFlowStepController() override = default;

  void Show(base::OnceCallback<void(bool)> step_shown_callback,
            bool reset_state) override {
    reauth_provider_->SwitchToReauth();
  }

  void OnHidden() override { host()->SetNativeToolbarVisible(false); }

  void OnNavigateBackRequested() override {
    NavigateBackInternal(reauth_provider_->contents());
  }

 private:
  std::unique_ptr<ProfilePickerDiceReauthProvider> reauth_provider_;
};

std::unique_ptr<ProfileManagementStepController> CreateReauthtep(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    base::OnceCallback<void(bool, const ForceSigninUIError&)>
        on_reauth_completed) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());

  return std::make_unique<ReauthFlowStepController>(
      host,
      std::make_unique<ProfilePickerDiceReauthProvider>(
          host, profile, entry->GetGAIAId(),
          base::UTF16ToUTF8(entry->GetUserName()),
          std::move(on_reauth_completed)),
      profile);
}
#endif

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
    ProfilePicker::ProfileInfo profile_info,
    StepSwitchFinishedCallback switch_finished_callback) {
  DCHECK_EQ(Step::kProfilePicker, current_step());

  base::FilePath profile_path;
  // Split the variant information from `profile_info`.
  absl::visit(base::Overloaded{
                  [&suggested_profile_color =
                       suggested_profile_color_](std::optional<SkColor> color) {
                    suggested_profile_color = color;
                  },
                  [&profile_path](base::FilePath profile_path_info) {
                    profile_path = profile_path_info;
                  },
              },
              profile_info);

  SwitchToIdentityStepsFromAccountSelection(std::move(switch_finished_callback),
                                            kAccessPoint,
                                            std::move(profile_path));
}

void ProfilePickerFlowController::SwitchToReauth(
    Profile* profile,
    base::OnceCallback<void(const ForceSigninUIError&)> on_error_callback) {
  DCHECK_EQ(Step::kProfilePicker, current_step());

  // if the step was already initialized, unregister to make sure the new
  // reauth is properly initialised and the current reauth step is cleaned.
  //
  // TODO(crbug.com/40280498): Cleanup the unregistration of the step with a
  // proper resetable state within the `ProfilePickerDiceReauthProvider`, and
  // using the `ProfileManagementFlowController::SwitchToStep()` `reset_state`
  // value to trigger the reset.
  if (IsStepInitialized(Step::kReauth)) {
    UnregisterStep(Step::kReauth);
  }

  RegisterStep(
      Step::kReauth,
      CreateReauthtep(
          host(), profile,
          base::BindOnce(&ProfilePickerFlowController::OnReauthCompleted,
                         base::Unretained(this), profile,
                         std::move(on_error_callback))));

  SwitchToStep(
      Step::kReauth, true, StepSwitchFinishedCallback(),
      /*pop_step_callback=*/CreateSwitchToStepPopCallback(current_step()));
}

void ProfilePickerFlowController::OnReauthCompleted(
    Profile* profile,
    base::OnceCallback<void(const ForceSigninUIError&)> on_error_callback,
    bool success,
    const ForceSigninUIError& error) {
  if (!success) {
    CHECK_NE(error.type(), ForceSigninUIError::Type::kNone);

    SwitchToStep(
        Step::kProfilePicker, /*reset_state=*/true,
        base::BindOnce(
            &ProfilePickerFlowController::OnProfilePickerStepShownReauthError,
            base::Unretained(this), std::move(on_error_callback), error));
    return;
  }

  g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .GetProfileAttributesWithPath(profile->GetPath())
      ->LockForceSigninProfile(false);

  FinishFlowAndRunInBrowser(profile, PostHostClearedCallback());
}

void ProfilePickerFlowController::OnProfilePickerStepShownReauthError(
    base::OnceCallback<void(const ForceSigninUIError&)> on_error_callback,
    const ForceSigninUIError& error,
    bool switch_step_success) {
  // If the step switch to the profile picker was not successful, do not proceed
  // with displaying the error dialog.
  if (!switch_step_success) {
    return;
  }

  std::move(on_error_callback).Run(error);
}

#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ProfilePickerFlowController::SwitchToPostSignIn(
    Profile* signed_in_profile,
    const CoreAccountInfo& account_info,
    std::optional<SkColor> profile_color,
    std::unique_ptr<content::WebContents> contents) {
  DCHECK_EQ(Step::kProfilePicker, current_step());
  suggested_profile_color_ = profile_color;
  SwitchToIdentityStepsFromPostSignIn(
      signed_in_profile, account_info,
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
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuManageProfiles:
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
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuAddNewProfile:
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
  }
}

std::u16string ProfilePickerFlowController::GetFallbackAccessibleWindowTitle()
    const {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return l10n_util::GetStringUTF16(IDS_PROFILE_PICKER_MAIN_VIEW_TITLE_LACROS);
#else
  return l10n_util::GetStringUTF16(IDS_PROFILE_PICKER_MAIN_VIEW_TITLE);
#endif
}

std::unique_ptr<ProfilePickerSignedInFlowController>
ProfilePickerFlowController::CreateSignedInFlowController(
    Profile* signed_in_profile,
    const CoreAccountInfo& account_info,
    std::unique_ptr<content::WebContents> contents) {
  DCHECK(!weak_signed_in_flow_controller_);

  created_profile_ = signed_in_profile->GetWeakPtr();
  auto step_completed_callback =
      base::BindOnce(&ProfilePickerFlowController::HandleIdentityStepsCompleted,
                     // Unretained ok: the callback is passed to a step that
                     // the `this` will own and outlive.
                     base::Unretained(this),
                     // Unretained ok: the steps register a profile keep-alive
                     // and will be alive until this callback runs.
                     base::Unretained(created_profile_.get()));

  auto signed_in_flow = std::make_unique<ProfileCreationSignedInFlowController>(
      host(), signed_in_profile, account_info, std::move(contents),
      suggested_profile_color_, std::move(step_completed_callback));
  weak_signed_in_flow_controller_ = signed_in_flow->GetWeakPtr();
  return signed_in_flow;
}

void ProfilePickerFlowController::SwitchToSignedOutPostIdentityFlow(
    Profile* profile,
    PostHostClearedCallback post_host_cleared_callback,
    StepSwitchFinishedCallback step_switch_finished_callback) {
  CHECK(profile);
  created_profile_ = profile->GetWeakPtr();
  CreateSignedOutFlowWebContents(created_profile_.get());

  HandleIdentityStepsCompleted(
      created_profile_.get(), std::move(post_host_cleared_callback),
      /*is_continue_callback=*/false, std::move(step_switch_finished_callback));
}

base::queue<ProfileManagementFlowController::Step>
ProfilePickerFlowController::RegisterPostIdentitySteps(
    PostHostClearedCallback post_host_cleared_callback) {
  CHECK(created_profile_);
  base::queue<ProfileManagementFlowController::Step> post_identity_steps;

  content::WebContents* web_contents = nullptr;
  if (weak_signed_in_flow_controller_) {
    // TODO(crbug.com/40942098): Find a way to get the web contents without
    // relying on the weak ptr.
    web_contents = weak_signed_in_flow_controller_->contents();
    CHECK(web_contents, base::NotFatalUntil::M127);
  } else {
    // TODO(crbug.com/40942098): Find another way to fetch the web contents.
    web_contents = GetSignedOutFlowWebContents();
    CHECK(web_contents, base::NotFatalUntil::M127);
  }

  auto search_engine_choice_step_completed = base::BindOnce(
      &ProfilePickerFlowController::AdvanceToNextPostIdentityStep,
      base::Unretained(this));
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(
          created_profile_.get());
  RegisterStep(
      Step::kSearchEngineChoice,
      ProfileManagementStepController::CreateForSearchEngineChoice(
          host(), search_engine_choice_dialog_service, web_contents,
          SearchEngineChoiceDialogService::EntryPoint::kProfileCreation,
          std::move(search_engine_choice_step_completed)));
  post_identity_steps.emplace(
      ProfileManagementFlowController::Step::kSearchEngineChoice);

  RegisterStep(
      Step::kFinishFlow,
      ProfileManagementStepController::CreateForFinishFlowAndRunInBrowser(
          host(),
          base::BindOnce(
              &ProfilePickerFlowController::FinishFlowAndRunInBrowser,
              base::Unretained(this), base::Unretained(created_profile_.get()),
              std::move(post_host_cleared_callback))));
  post_identity_steps.emplace(
      ProfileManagementFlowController::Step::kFinishFlow);

  return post_identity_steps;
}
