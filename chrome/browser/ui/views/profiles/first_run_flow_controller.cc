// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/first_run_flow_controller.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/version_info/channel.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_hats_util.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/default_browser_step_eligibility_checker.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_eligibility_tracker.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_step_eligibility_checker.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/password_manager_feature_showcase_eligibility_checker.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_post_sign_in_adapter.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/feature_showcase/feature_showcase_ui.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/core_account_id.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "base/check_deref.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "components/application_locale_storage/application_locale_storage.h"
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace {

const signin_metrics::AccessPoint kAccessPoint =
    signin_metrics::AccessPoint::kForYouFre;

void MaybeLogSetAsDefaultSuccess(
    shell_integration::DefaultWebClientState state) {
  if (state == shell_integration::IS_DEFAULT) {
    base::UmaHistogramEnumeration(
        "ProfilePicker.FirstRun.DefaultBrowser",
        DefaultBrowserChoice::kSuccessfullySetAsDefault);
  }
}

bool IsPostIdentityStep(ProfileManagementFlowController::Step step) {
  switch (step) {
    case ProfileManagementFlowController::Step::kUnknown:
    case ProfileManagementFlowController::Step::kFinishFlow:
    case ProfileManagementFlowController::Step::kFinishSamlSignin:
    case ProfileManagementFlowController::Step::kPostSignInFlow:
    case ProfileManagementFlowController::Step::kProfilePicker:
    case ProfileManagementFlowController::Step::kAccountSelection:
    case ProfileManagementFlowController::Step::kIntro:
    case ProfileManagementFlowController::Step::kReauth:
      return false;
    case ProfileManagementFlowController::Step::kDefaultBrowser:
    case ProfileManagementFlowController::Step::kSearchEngineChoice:
    case ProfileManagementFlowController::Step::kFeatureShowcase:
      return true;
  }
}

bool IsProfileInSearchEngineChoiceRegion(Profile* profile) {
  return CHECK_DEREF(regional_capabilities::RegionalCapabilitiesServiceFactory::
                         GetForProfile(profile))
      .IsInSearchEngineChoiceScreenRegion();
}

std::optional<std::vector<std::string>> GetForcedStepsFromCommandLine() {
  const base::CommandLine& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());
  if (command_line.HasSwitch(switches::kForceFreFeatureShowcaseSteps)) {
    std::string steps_str = command_line.GetSwitchValueASCII(
        switches::kForceFreFeatureShowcaseSteps);
    return base::SplitString(steps_str, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  }
  return std::nullopt;
}

#if BUILDFLAG(IS_WIN)
void PinToTaskbarResult(bool result) {
  base::UmaHistogramBoolean("Windows.TaskbarPinFromFRESucceeded", result);
}
#endif  // BUILDFLAG(IS_WIN)

class IntroStepController : public ProfileManagementStepController {
 public:
  explicit IntroStepController(
      ProfilePickerWebContentsHost* host,
      base::RepeatingCallback<void(IntroChoice)> choice_callback,
      bool enable_animations,
      base::RepeatingCallback<bool()> query_effects_callback)
      : ProfileManagementStepController(host),
        intro_url_(BuildIntroURL(enable_animations)),
        choice_callback_(std::move(choice_callback)),
        query_effects_callback_(std::move(query_effects_callback)) {}

  ~IntroStepController() override = default;

  void Show(StepSwitchFinishedCallback step_shown_callback,
            bool reset_state) override {
    if (reset_state) {
      // Reload the WebUI in the picker contents.
      host()->ShowScreenInPickerContents(
          intro_url_, base::BindOnce(&IntroStepController::OnIntroLoaded,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(step_shown_callback)));
    } else {
      // Just switch to the picker contents, which should be showing this step.
      DCHECK_EQ(intro_url_, host()->GetPickerContents()->GetURL());
      host()->ShowScreenInPickerContents(
          GURL(), base::BindOnce(std::move(step_shown_callback.value()), true));
      ExpectSigninChoiceOnce();
      UpdateAnimationsState();
    }
  }

  void OnNavigateBackRequested() override {
    NavigateBackInternal(host()->GetPickerContents());
  }

  void OnIntroLoaded(StepSwitchFinishedCallback step_shown_callback) {
    std::move(step_shown_callback.value()).Run(/*success=*/true);

    ExpectSigninChoiceOnce();
    UpdateAnimationsState();
  }

  void ToggleMediaEffects(bool active) override {
    UpdateAnimationsState(active);
  }

 private:
  GURL BuildIntroURL(bool enable_animations) {
    std::string url_string = chrome::kChromeUIIntroURL;
    if (!enable_animations) {
      url_string += "?noAnimations";
    }
    return GURL(url_string);
  }

  void ExpectSigninChoiceOnce() {
    auto* intro_ui = host()
                         ->GetPickerContents()
                         ->GetWebUI()
                         ->GetController()
                         ->GetAs<IntroUI>();
    DCHECK(intro_ui);
    intro_ui->SetSigninChoiceCallback(
        IntroSigninChoiceCallback(choice_callback_));
  }

  void UpdateAnimationsState() {
    UpdateAnimationsState(query_effects_callback_.Run());
  }

  void UpdateAnimationsState(bool active) {
    auto* intro_ui = host()
                         ->GetPickerContents()
                         ->GetWebUI()
                         ->GetController()
                         ->GetAs<IntroUI>();
    if (intro_ui) {
      intro_ui->ToggleAnimations(active);
    }
  }

  const GURL intro_url_;

  // `choice_callback_` is a `Repeating` one to be able to advance the flow more
  // than once in case we navigate back to this step.
  const base::RepeatingCallback<void(IntroChoice)> choice_callback_;

  const base::RepeatingCallback<bool()> query_effects_callback_;

  base::WeakPtrFactory<IntroStepController> weak_ptr_factory_{this};
};

class DefaultBrowserStepController : public ProfileManagementStepController {
 public:
  explicit DefaultBrowserStepController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      base::OnceClosure step_completed_callback)
      : ProfileManagementStepController(host),
        profile_(CHECK_DEREF(profile)),
        step_completed_callback_(std::move(step_completed_callback)) {}

  ~DefaultBrowserStepController() override {
    if (step_completed_callback_) {
      base::UmaHistogramEnumeration("ProfilePicker.FirstRun.DefaultBrowser",
                                    DefaultBrowserChoice::kQuit);
    }
  }

  void Show(StepSwitchFinishedCallback step_shown_callback,
            bool reset_state) override {
    CHECK(!step_shown_callback->is_null());
    CHECK(reset_state);

    step_shown_callback_ = std::move(step_shown_callback);

    show_default_browser_screen_callback_ =
        base::BindOnce(&DefaultBrowserStepController::ShowDefaultBrowserScreen,
                       weak_ptr_factory_.GetWeakPtr());

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (CHECK_DEREF(command_line)
            .HasSwitch(switches::kForceFreDefaultBrowserStep)) {
      OnEligibilityDetermined(true);
      return;
    }

    timeout_closure_.Reset(base::BindOnce(
        &DefaultBrowserStepController::OnDefaultBrowserCheckTimeout,
        weak_ptr_factory_.GetWeakPtr()));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, timeout_closure_.callback(), base::Seconds(2));

    checker_.CheckEligibility(
        *profile_,
        base::BindOnce(&DefaultBrowserStepController::OnEligibilityDetermined,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnNavigateBackRequested() override {
    // Do nothing, navigating back is not allowed.
  }

 private:
  void OnDefaultBrowserCheckTimeout() {
    if (!step_completed_callback_) {
      return;
    }

    base::UmaHistogramEnumeration("ProfilePicker.FirstRun.DefaultBrowser",
                                  DefaultBrowserChoice::kNotShownOnTimeout);
    // Mark that this step was skipped and proceed with the next one.
    std::move(step_shown_callback_.value()).Run(false);
    std::move(step_completed_callback_).Run();
    show_default_browser_screen_callback_.Reset();
  }

  void OnEligibilityDetermined(bool is_eligible) {
    if (!show_default_browser_screen_callback_) {
      return;
    }

    timeout_closure_.Cancel();

    if (is_eligible) {
#if BUILDFLAG(IS_WIN)
      // Check if Chrome can pin to the taskbar, which is an async call. When it
      // finishes, the result will be recorded and
      // `show_default_browser_screen_callback_` will be run.
      browser_util::ShouldOfferToPin(
          ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
          browser_util::PinAppToTaskbarChannel::kFirstRunExperience,
          base::BindOnce(&DefaultBrowserStepController::OnCanPinToTaskbarResult,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
#else
      std::move(show_default_browser_screen_callback_).Run(/*can_pin=*/false);
#endif  // BUILDFLAG(IS_WIN)
    } else {
      // Mark that this step was skipped and proceed with the next one.
      std::move(step_shown_callback_.value()).Run(false);
      std::move(step_completed_callback_).Run();
    }
  }

  void OnLoadFinished(bool can_pin) {
    auto* intro_ui = host()
                         ->GetPickerContents()
                         ->GetWebUI()
                         ->GetController()
                         ->GetAs<IntroUI>();
    CHECK(intro_ui);
    if (can_pin) {
      intro_ui->SetCanPinToTaskbar(can_pin);
    }
    intro_ui->SetDefaultBrowserCallback(DefaultBrowserCallback(
        base::BindOnce(&DefaultBrowserStepController::OnStepCompleted,
                       // WeakPtr: The callback is given to the WebUIController,
                       // owned by the webcontents, which lifecycle is not
                       // bounded by a single step.
                       weak_ptr_factory_.GetWeakPtr(), can_pin)));
  }

  void OnCanPinToTaskbarResult(bool can_pin) {
    std::move(show_default_browser_screen_callback_).Run(can_pin);
  }

  void OnStepCompleted(bool can_pin, DefaultBrowserChoice choice) {
    if (choice == DefaultBrowserChoice::kClickSetAsDefault) {
      // The worker pointer is reference counted. While it is running, sequence
      // it runs on will hold references to it and it will be automatically
      // freed once all its tasks have finished.
      base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
          ->StartSetAsDefault(base::BindOnce(&MaybeLogSetAsDefaultSuccess));
#if BUILDFLAG(IS_WIN)
      if (can_pin) {
        browser_util::PinAppToTaskbar(
            ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
            browser_util::PinAppToTaskbarChannel::kFirstRunExperience,
            base::BindOnce(&PinToTaskbarResult));
      }
#endif  // BUILDFLAG(IS_WIN)
    }
    base::UmaHistogramEnumeration("ProfilePicker.FirstRun.DefaultBrowser",
                                  choice);
    CHECK(step_completed_callback_);
    std::move(step_completed_callback_).Run();
  }

  void ShowDefaultBrowserScreen(bool can_pin) {
    base::OnceClosure navigation_finished_closure =
        base::BindOnce(&DefaultBrowserStepController::OnLoadFinished,
                       base::Unretained(this), can_pin);

    if (!step_shown_callback_->is_null()) {
      // Notify the previous step before executing this step's initialization
      // callback.
      navigation_finished_closure =
          base::BindOnce(std::move(step_shown_callback_.value()), true)
              .Then(std::move(navigation_finished_closure));
    }

    host()->ShowScreenInPickerContents(
        GURL(chrome::kChromeUIIntroDefaultBrowserURL),
        std::move(navigation_finished_closure));
  }

  raw_ref<Profile> profile_;
  DefaultBrowserStepEligibilityChecker checker_;

  // Callback to be executed when the step is completed.
  base::OnceClosure step_completed_callback_;
  StepSwitchFinishedCallback step_shown_callback_;

  base::OnceCallback<void(bool)> show_default_browser_screen_callback_;
  base::CancelableOnceClosure timeout_closure_;
  base::WeakPtrFactory<DefaultBrowserStepController> weak_ptr_factory_{this};
};

class FeatureShowcaseStepController : public ProfileManagementStepController {
 public:
  explicit FeatureShowcaseStepController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      base::OnceClosure step_completed_callback)
      : ProfileManagementStepController(host),
        profile_(profile),
        step_completed_callback_(std::move(step_completed_callback)) {
    std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>>
        checkers;

    // Register checkers in order of priority (highest first).
    checkers.push_back(
        std::make_unique<PasswordManagerFeatureShowcaseEligibilityChecker>());
    tracker_ = std::make_unique<FeatureShowcaseEligibilityTracker>(
        std::move(checkers));
  }

  ~FeatureShowcaseStepController() override = default;

  void Show(StepSwitchFinishedCallback step_shown_callback,
            bool reset_state) override {
    CHECK(reset_state);

    step_shown_callback_ = std::move(step_shown_callback);

    if (std::optional<std::vector<std::string>> forced_steps =
            GetForcedStepsFromCommandLine();
        forced_steps) {
      OnEligibilityDetermined(*forced_steps);
      return;
    }

    tracker_->EvaluateEligibleSteps(
        *profile_,
        base::BindOnce(&FeatureShowcaseStepController::OnEligibilityDetermined,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnNavigateBackRequested() override {
    // Navigating back from post-identity steps is usually blocked.
    NOTREACHED();
  }

 private:
  void OnEligibilityDetermined(const std::vector<std::string>& eligible_steps) {
    if (eligible_steps.empty()) {
      std::move(step_shown_callback_.value()).Run(/*success=*/false);
      std::move(step_completed_callback_).Run();
      return;
    }

    host()->ShowScreenInPickerContents(
        BuildFeatureShowcaseURL(eligible_steps),
        base::BindOnce(&FeatureShowcaseStepController::OnLoadFinished,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  GURL BuildFeatureShowcaseURL(const std::vector<std::string>& steps) {
    GURL url(chrome::kChromeUIFeatureShowcaseURL);
    return net::AppendQueryParameter(url, "steps",
                                     base::JoinString(steps, ","));
  }

  void OnLoadFinished() {
    if (!step_shown_callback_->is_null()) {
      std::move(step_shown_callback_.value()).Run(/*success=*/true);
    }

    auto* showcase_ui = host()
                            ->GetPickerContents()
                            ->GetWebUI()
                            ->GetController()
                            ->GetAs<FeatureShowcaseUI>();
    CHECK(showcase_ui);

    showcase_ui->SetFinishCallback(
        base::BindOnce(&FeatureShowcaseStepController::OnStepCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnStepCompleted() {
    CHECK(step_completed_callback_);
    std::move(step_completed_callback_).Run();
  }

  raw_ptr<Profile> profile_;
  base::OnceClosure step_completed_callback_;
  StepSwitchFinishedCallback step_shown_callback_;
  std::unique_ptr<FeatureShowcaseEligibilityTracker> tracker_;

  base::WeakPtrFactory<FeatureShowcaseStepController> weak_ptr_factory_{this};
};

using IdentityStepsCompletedCallback =
    base::OnceCallback<void(PostHostClearedCallback post_host_cleared_callback,
                            bool is_continue_callback)>;

// Instance allowing `TurnSyncOnHelper` to drive the interface in the
// `kPostSignIn` step.
class FirstRunPostSignInAdapter : public ProfilePickerPostSignInAdapter {
 public:
  FirstRunPostSignInAdapter(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents,
      IdentityStepsCompletedCallback step_completed_callback)
      : ProfilePickerPostSignInAdapter(host,
                                       profile,
                                       account_info,
                                       std::move(contents),
                                       kAccessPoint,
                                       /*profile_color=*/std::nullopt),
        step_completed_callback_(std::move(step_completed_callback)) {
    DCHECK(step_completed_callback_);
  }

  void Init(StepSwitchFinishedCallback step_switch_callback) override {
    // Stop with the sign-in navigation and show a spinner instead. The spinner
    // will be shown until TurnSyncOnHelper figures out whether it's a
    // managed account and whether sync is disabled by policies (which in some
    // cases involves fetching policies and can take a couple of seconds).
    host()->ShowScreen(contents(), GetSyncConfirmationURL(/*loading=*/true),
                       /*navigation_finished_closure=*/base::OnceClosure());

    ProfilePickerPostSignInAdapter::Init(std::move(step_switch_callback));
  }

  PostHostClearedCallback CreateSupervisedUserIphCallback() {
    return PostHostClearedCallback(
        base::BindOnce([](BrowserWindowInterface* browser) {
          CHECK(browser);
          BrowserView* browser_view =
              BrowserView::GetBrowserViewForBrowser(browser);
          if (!browser_view) {
            return;
          }
          browser_view->MaybeShowSupervisedUserProfileSignInIPH();
        }));
  }

  void FinishAndOpenBrowserInternal(
      PostHostClearedCallback post_host_cleared_callback,
      bool is_continue_callback) override {
    // Do nothing if this has already been called. Note that this can get called
    // first time from a special case handling (such as the Settings link) and
    // than second time when the TurnSyncOnHelper finishes.
    if (!step_completed_callback_) {
      return;
    }
    // The supervised user IPH should be called after the present
    // post_host_cleared_callback which finishes the browser creation.
    std::vector<PostHostClearedCallback> callbacks;
    callbacks.push_back(std::move(post_host_cleared_callback));
    callbacks.push_back(CreateSupervisedUserIphCallback());
    auto combined_callback =
        CombineCallbacks<PostHostClearedCallback, BrowserWindowInterface*>(
            std::move(callbacks));
    std::move(step_completed_callback_)
        .Run(std::move(combined_callback), is_continue_callback);
  }

 private:
  IdentityStepsCompletedCallback step_completed_callback_;
};

}  // namespace

std::unique_ptr<ProfileManagementStepController> CreateIntroStep(
    ProfilePickerWebContentsHost* host,
    base::RepeatingCallback<void(IntroChoice)> choice_callback,
    bool enable_animations,
    base::RepeatingCallback<bool()> query_effects_callback) {
  return std::make_unique<IntroStepController>(
      host, std::move(choice_callback), enable_animations,
      std::move(query_effects_callback));
}

std::unique_ptr<ProfileManagementStepController> CreateDefaultBrowserStep(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    base::OnceClosure step_completed_callback) {
  return std::make_unique<DefaultBrowserStepController>(
      host, profile, std::move(step_completed_callback));
}

std::unique_ptr<ProfileManagementStepController> CreateFeatureShowcaseStep(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    base::OnceClosure step_completed_callback) {
  return std::make_unique<FeatureShowcaseStepController>(
      host, profile, std::move(step_completed_callback));
}

FirstRunFlowController::FirstRunFlowController(
    ProfilePickerWebContentsHost* host,
    ClearHostClosure clear_host_callback,
    Profile* profile,
    ProfilePicker::FirstRunExitedCallback first_run_exited_callback)
    : ProfileManagementFlowControllerImpl(host,
                                          std::move(clear_host_callback),
                                          /*flow_type_string=*/"FREFlow"),
      profile_(profile),
      first_run_exited_callback_(std::move(first_run_exited_callback)) {
  DCHECK(profile_);
  DCHECK(first_run_exited_callback_);
}

FirstRunFlowController::~FirstRunFlowController() {
  if (!first_run_exited_callback_) {
    // As the callback gets executed by `PreFinishWithBrowser()`,
    // this indicates that `FinishFlowAndRunInBrowser()` has already run.
    return;
  }

  // The core of the flow stops at the sync opt in step. Considering the flow
  // completed means among other things that we would always proceed to the
  // browser when closing the host view.
  bool is_core_flow_completed = IsPostIdentityStep(current_step());

  if (is_core_flow_completed) {
    RunFinishFlowCallback();
  } else {
    // TODO(crbug.com/40276516): Revisit the enum value name for kQuitAtEnd.
    std::move(first_run_exited_callback_)
        .Run(ProfilePicker::FirstRunExitStatus::kQuitAtEnd);
  }
}

void FirstRunFlowController::ShowSigninError(Profile* profile,
                                             const SigninUIError& error) {
  base::UmaHistogramEnumeration("ProfilePicker.FREFlow.SignInError",
                                error.type());
  // Display the signin error once the browser opens.
  HandleSigninErrorInBrowser(profile, error);
}

void FirstRunFlowController::ToggleMediaEffects(bool active) {
  if (ProfileManagementStepController* current_step_controller =
          GetCurrentStepController()) {
    current_step_controller->ToggleMediaEffects(active);
  }
  // TODO(crbug.com/485150597): Add resuming/pausing the audio.
}

bool FirstRunFlowController::AreEffectsEnabled() const {
  return host()->AreEffectsEnabled();
}

void FirstRunFlowController::Init() {
  RegisterStep(
      Step::kIntro,
      CreateIntroStep(
          host(),
          base::BindRepeating(&FirstRunFlowController::HandleIntroSigninChoice,
                              weak_ptr_factory_.GetWeakPtr()),
          /*enable_animations=*/true,
          base::BindRepeating(&FirstRunFlowController::AreEffectsEnabled,
                              base::Unretained(this))));
  SwitchToStep(Step::kIntro, /*reset_state=*/true);

  signin_metrics::LogSignInOffered(
      kAccessPoint, signin_metrics::PromoAction::
                        PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT);
}

void FirstRunFlowController::CancelSigninFlow() {
  // Called when the user declines enterprise management. Unfortunately, for
  // some technical and historical reasons, management is already marked as
  // accepted before we show the prompt. So here we need to revert it.
  // Currently we remove the account to match the behaviour from the profile
  // creation flow.
  // TODO(crbug.com/40067597): Look into letting the user keep their account.
  signin::ClearProfileWithManagedAccounts(profile_);

  HandleIdentityStepsCompleted(profile_, PostHostClearedCallback(),
                               /*is_continue_callback=*/false);
}

void FirstRunFlowController::PickProfile(
    const base::FilePath& profile_path,
    ProfilePicker::ProfilePickingArgs args,
    base::OnceCallback<void(bool)> pick_profile_complete_callback) {
  NOTREACHED() << "FRE is not expected to handle this flow";
}

bool FirstRunFlowController::PreFinishWithBrowser() {
  DCHECK(first_run_exited_callback_);
  std::move(first_run_exited_callback_)
      .Run(ProfilePicker::FirstRunExitStatus::kCompleted);

  MaybeTriggerHatsSurvey();

  return true;
}

void FirstRunFlowController::HandleIntroSigninChoice(IntroChoice choice) {
  if (choice == IntroChoice::kQuit) {
    // The view is getting destroyed. The class destructor will handle the rest.
    return;
  }

  if (choice == IntroChoice::kContinueWithoutAccount) {
    HandleIdentityStepsCompleted(profile_, PostHostClearedCallback(),
                                 /*is_continue_callback=*/false);
    return;
  }

  SwitchToIdentityStepsFromAccountSelection(
      /*step_switch_finished_callback=*/StepSwitchFinishedCallback(),
      kAccessPoint, profile_->GetPath());
}

std::unique_ptr<ProfilePickerPostSignInAdapter>
FirstRunFlowController::CreatePostSignInAdapter(
    Profile* signed_in_profile,
    const CoreAccountInfo& account_info,
    std::unique_ptr<content::WebContents> contents) {
  DCHECK_EQ(profile_, signed_in_profile);
  return std::make_unique<FirstRunPostSignInAdapter>(
      host(), signed_in_profile, account_info, std::move(contents),
      base::BindOnce(&FirstRunFlowController::HandleIdentityStepsCompleted,
                     // Unretained ok: the callback is passed to a step that
                     // the `this` will own and outlive.
                     base::Unretained(this), base::Unretained(profile_)));
}

void FirstRunFlowController::RunFinishFlowCallback() {
  if (finish_flow_callback_) {
    std::move(finish_flow_callback_).Run();
  }
}

std::string FirstRunFlowController::GetHatsSurveyTrigger() const {
  if (switches::IsFirstRunDesktopRefreshEnabled(
          IsProfileInSearchEngineChoiceRegion(profile_))) {
    return kHatsSurveyTriggerIdentityRefreshedFirstRunCompleted;
  }

  return kHatsSurveyTriggerIdentityFirstRunCompleted;
}

void FirstRunFlowController::MaybeTriggerHatsSurvey() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // No variations seed is available on Mac and Linux at the very first run of
  // Chrome. Check the locale manually to make sure the survey is enabled for
  // only eligible users. Do the locale check before the feature check to avoid
  // unnecessary feature activation.
  const ApplicationLocaleStorage& application_locale_storage =
      CHECK_DEREF(CHECK_DEREF(CHECK_DEREF(g_browser_process).GetFeatures())
                      .application_locale_storage());
  if (application_locale_storage.Get() != "en-US") {
    return;
  }
#endif

  if (current_step() != Step::kFinishFlow) {
    // Skip the survey if the user did not complete the flow.
    return;
  }

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager) {
    return;
  }
  const AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  if (account_info.IsEmpty() ||
      account_info.IsManaged() != signin::Tribool::kFalse) {
    // Skip the survey for not signed in users and for managed users.
    return;
  }

  std::map<std::string, std::string> data = {
      {"Channel",
       std::string(version_info::GetChannelString(chrome::GetChannel()))}};
  signin::LaunchHatsSurveyForProfile(GetHatsSurveyTrigger(), profile_,
                                     /*defer_if_no_browser=*/true,
                                     std::move(data));
}

base::queue<ProfileManagementFlowController::Step>
FirstRunFlowController::RegisterPostIdentitySteps(
    PostHostClearedCallback post_host_cleared_callback) {
  base::queue<ProfileManagementFlowController::Step> post_identity_steps;

  finish_flow_callback_ = base::BindOnce(
      &FirstRunFlowController::FinishFlowAndRunInBrowser,
      base::Unretained(this),
      // Unretained ok: the steps register a profile keep-alive and
      // will be alive until this callback runs.
      base::Unretained(profile_), std::move(post_host_cleared_callback));

  auto search_engine_choice_step_completed =
      base::BindOnce(&FirstRunFlowController::AdvanceToNextPostIdentityStep,
                     base::Unretained(this));
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(profile_);
  RegisterStep(
      Step::kSearchEngineChoice,
      ProfileManagementStepController::CreateForSearchEngineChoice(
          host(), search_engine_choice_dialog_service,
          host()->GetPickerContents(),
          SearchEngineChoiceDialogService::EntryPoint::kFirstRunExperience,
          std::move(search_engine_choice_step_completed)));
  post_identity_steps.emplace(
      ProfileManagementFlowController::Step::kSearchEngineChoice);

  auto default_browser_promo_step_completed =
      base::BindOnce(&FirstRunFlowController::AdvanceToNextPostIdentityStep,
                     base::Unretained(this));
  RegisterStep(
      Step::kDefaultBrowser,
      CreateDefaultBrowserStep(
          host(), profile_, std::move(default_browser_promo_step_completed)));
  post_identity_steps.emplace(
      ProfileManagementFlowController::Step::kDefaultBrowser);

  const bool is_desktop_revamp_enabled =
      switches::IsFirstRunDesktopRevampEnabled(
          IsProfileInSearchEngineChoiceRegion(profile_));
  if (is_desktop_revamp_enabled) {
    auto feature_showcase_step_completed =
        base::BindOnce(&FirstRunFlowController::AdvanceToNextPostIdentityStep,
                       base::Unretained(this));
    RegisterStep(
        Step::kFeatureShowcase,
        CreateFeatureShowcaseStep(host(), profile_,
                                  std::move(feature_showcase_step_completed)));
    post_identity_steps.emplace(
        ProfileManagementFlowController::Step::kFeatureShowcase);
  }

  RegisterStep(
      Step::kFinishFlow,
      ProfileManagementStepController::CreateForFinishFlowAndRunInBrowser(
          host(), base::BindOnce(&FirstRunFlowController::RunFinishFlowCallback,
                                 base::Unretained(this))));
  post_identity_steps.emplace(
      ProfileManagementFlowController::Step::kFinishFlow);
  return post_identity_steps;
}
