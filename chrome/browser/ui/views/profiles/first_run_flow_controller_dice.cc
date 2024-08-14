// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/first_run_flow_controller_dice.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/gurl.h"

namespace {

constexpr base::TimeDelta kDefaultBrowserCheckTimeout = base::Seconds(2);

const signin_metrics::AccessPoint kAccessPoint =
    signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE;

enum class ShowDefaultBrowserStep {
  // The default browser step should be shown as appropriate.
  kYes,
  // The default browser step should be skipped.
  kNo,
  // The default browser step should be shown even if we normally should skip
  // it, example because of policies or the current default state.
  kForce
};

bool IsDefaultBrowserDisabledByPolicy() {
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(
          prefs::kDefaultBrowserSettingEnabled);
  CHECK(pref);
  DCHECK(pref->GetValue()->is_bool());
  return pref->IsManaged() && !pref->GetValue()->GetBool();
}

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
      return true;
  }
}

class IntroStepController : public ProfileManagementStepController {
 public:
  explicit IntroStepController(
      ProfilePickerWebContentsHost* host,
      base::RepeatingCallback<void(IntroChoice)> choice_callback,
      bool enable_animations)
      : ProfileManagementStepController(host),
        intro_url_(BuildIntroURL(enable_animations)),
        choice_callback_(std::move(choice_callback)) {}

  ~IntroStepController() override = default;

  void Show(base::OnceCallback<void(bool success)> step_shown_callback,
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
      host()->ShowScreenInPickerContents(GURL());
      ExpectSigninChoiceOnce();
    }
  }

  void OnNavigateBackRequested() override {
    NavigateBackInternal(host()->GetPickerContents());
  }

  void OnIntroLoaded(base::OnceCallback<void(bool)> step_shown_callback) {
    if (step_shown_callback) {
      std::move(step_shown_callback).Run(/*success=*/true);
    }

    ExpectSigninChoiceOnce();
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

  const GURL intro_url_;

  // `choice_callback_` is a `Repeating` one to be able to advance the flow more
  // than once in case we navigate back to this step.
  const base::RepeatingCallback<void(IntroChoice)> choice_callback_;

  base::WeakPtrFactory<IntroStepController> weak_ptr_factory_{this};
};

class DefaultBrowserStepController : public ProfileManagementStepController {
 public:
  explicit DefaultBrowserStepController(
      ProfilePickerWebContentsHost* host,
      base::OnceCallback<void(StepSwitchFinishedCallback)>
          step_completed_callback)
      : ProfileManagementStepController(host),
        step_completed_callback_(std::move(step_completed_callback)) {}

  ~DefaultBrowserStepController() override {
    if (step_completed_callback_) {
      base::UmaHistogramEnumeration("ProfilePicker.FirstRun.DefaultBrowser",
                                    DefaultBrowserChoice::kQuit);
    }
  }

  void Show(base::OnceCallback<void(bool success)> step_shown_callback,
            bool reset_state) override {
    CHECK(reset_state);
    const ShowDefaultBrowserStep show_screen = ShouldShowScreen();

    if (show_screen == ShowDefaultBrowserStep::kNo) {
      // Forward the callback since the step is skipped.
      std::move(step_completed_callback_).Run(std::move(step_shown_callback));
      return;
    }

    switch_from_previous_step_finished_callback_ =
        std::move(step_shown_callback);
    navigation_finished_closure_ = base::BindOnce(
        &DefaultBrowserStepController::OnLoadFinished, base::Unretained(this));

    show_default_browser_screen_callback_ =
        base::BindOnce(&DefaultBrowserStepController::ShowDefaultBrowserScreen,
                       weak_ptr_factory_.GetWeakPtr());

    // If the feature is set to forced, show the step even if it's already
    // the default browser.
    if (show_screen == ShowDefaultBrowserStep::kForce) {
      std::move(show_default_browser_screen_callback_).Run();
      return;
    }

    // Set up the timeout closure, in case checking if the browser is already
    // set as default isn't completed before the timeout.
    default_browser_check_timeout_closure_.Reset(base::BindOnce(
        &DefaultBrowserStepController::OnDefaultBrowserCheckTimeout,
        weak_ptr_factory_.GetWeakPtr()));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, default_browser_check_timeout_closure_.callback(),
        kDefaultBrowserCheckTimeout);

    // Check if browser is already set as default. If it isn't, show default
    // browser step.
    base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
        ->StartCheckIsDefault(base::BindOnce(
            &DefaultBrowserStepController::OnDefaultBrowserCheckFinished,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void OnNavigateBackRequested() override {
    // Do nothing, navigating back is not allowed.
  }

 private:
  void OnLoadFinished() {
    auto* intro_ui = host()
                         ->GetPickerContents()
                         ->GetWebUI()
                         ->GetController()
                         ->GetAs<IntroUI>();
    CHECK(intro_ui);
    intro_ui->SetDefaultBrowserCallback(DefaultBrowserCallback(
        base::BindOnce(&DefaultBrowserStepController::OnStepCompleted,
                       // WeakPtr: The callback is given to the WebUIController,
                       // owned by the webcontents, which lifecycle is not
                       // bounded by a single step.
                       weak_ptr_factory_.GetWeakPtr())));
  }

  void OnStepCompleted(DefaultBrowserChoice choice) {
    if (choice == DefaultBrowserChoice::kClickSetAsDefault) {
      CHECK(!IsDefaultBrowserDisabledByPolicy());
      // The worker pointer is reference counted. While it is running, sequence
      // it runs on will hold references to it and it will be automatically
      // freed once all its tasks have finished.
      base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
          ->StartSetAsDefault(base::BindOnce(&MaybeLogSetAsDefaultSuccess));
    }
    base::UmaHistogramEnumeration("ProfilePicker.FirstRun.DefaultBrowser",
                                  choice);
    CHECK(step_completed_callback_);
    std::move(step_completed_callback_).Run(StepSwitchFinishedCallback());
  }

  void OnDefaultBrowserCheckFinished(
      shell_integration::DefaultWebClientState state) {
    if (!show_default_browser_screen_callback_) {
      return;
    }

    // Cancel timeout.
    default_browser_check_timeout_closure_.Cancel();

    bool should_show_default_browser_step =
        state == shell_integration::NOT_DEFAULT ||
        state == shell_integration::OTHER_MODE_IS_DEFAULT;

    if (should_show_default_browser_step) {
      std::move(show_default_browser_screen_callback_).Run();
    } else {
      std::move(step_completed_callback_)
          .Run(std::move(switch_from_previous_step_finished_callback_));
    }
  }

  void OnDefaultBrowserCheckTimeout() {
    if (!step_completed_callback_) {
      return;
    }

    base::UmaHistogramEnumeration("ProfilePicker.FirstRun.DefaultBrowser",
                                  DefaultBrowserChoice::kNotShownOnTimeout);
    std::move(step_completed_callback_)
        .Run(std::move(switch_from_previous_step_finished_callback_));
  }

  void ShowDefaultBrowserScreen() {
    if (navigation_finished_closure_) {
      if (switch_from_previous_step_finished_callback_) {
        // Notify the previous step before executing this step's initialization
        // callback.
        navigation_finished_closure_ =
            base::BindOnce(
                std::move(switch_from_previous_step_finished_callback_), true)
                .Then(std::move(navigation_finished_closure_));
      }

      host()->ShowScreenInPickerContents(
          GURL(chrome::kChromeUIIntroDefaultBrowserURL),
          std::move(navigation_finished_closure_));
    }
  }

  ShowDefaultBrowserStep ShouldShowScreen() const {
    bool should_show_default_browser_step =
        // Check for policies.
        !IsDefaultBrowserDisabledByPolicy() &&
        // Some releases cannot be set as default browser.
        shell_integration::CanSetAsDefaultBrowser();

    if (!should_show_default_browser_step) {
      return ShowDefaultBrowserStep::kNo;
    }

    // The default browser step should be shown only on Windows. We only show it
    // on Windows because we display a dialog before the FRE on MacOS and Linux
    // to ask the user about the default browser. If it's forced, it should be
    // shown on the other platforms for testing.
#if BUILDFLAG(IS_WIN)
    return ShowDefaultBrowserStep::kYes;
#else
    // Non-Windows platforms should not show this unless forced (e.g.
    // command line)
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    return command_line->HasSwitch(switches::kForceFreDefaultBrowserStep)
               ? ShowDefaultBrowserStep::kForce
               : ShowDefaultBrowserStep::kNo;
#endif  // BUILDFLAG(IS_WIN)
  }

  // Callback to be executed when the step is completed.
  base::OnceCallback<void(StepSwitchFinishedCallback)> step_completed_callback_;

  // Callback to be expected when switching from the previous step to this step
  // is completed. If this step is skipped, we should forward it to
  // `step_completed_callback_`.
  StepSwitchFinishedCallback switch_from_previous_step_finished_callback_;

  base::OnceClosure navigation_finished_closure_;
  base::CancelableOnceClosure default_browser_check_timeout_closure_;
  base::OnceCallback<void()> show_default_browser_screen_callback_;
  base::WeakPtrFactory<DefaultBrowserStepController> weak_ptr_factory_{this};
};

using IdentityStepsCompletedCallback = base::OnceCallback<void(
    PostHostClearedCallback post_host_cleared_callback,
    bool is_continue_callback,
    StepSwitchFinishedCallback step_switch_finished_callback)>;

// Instance allowing `TurnSyncOnHelper` to drive the interface in the
// `kPostSignIn` step.
//
// Not following the `*SignedInFlowController` naming pattern to avoid confusion
// with `*StepController` and `*FlowController` that we also have here.
// `ProfilePickerSignedInFlowController` should eventually be renamed.
class FirstRunPostSignInAdapter : public ProfilePickerSignedInFlowController {
 public:
  FirstRunPostSignInAdapter(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents,
      IdentityStepsCompletedCallback step_completed_callback)
      : ProfilePickerSignedInFlowController(host,
                                            profile,
                                            account_info,
                                            std::move(contents),
                                            kAccessPoint,
                                            /*profile_color=*/std::nullopt),
        step_completed_callback_(std::move(step_completed_callback)) {
    DCHECK(step_completed_callback_);
  }

  void Init() override {
    // Stop with the sign-in navigation and show a spinner instead. The spinner
    // will be shown until TurnSyncOnHelper figures out whether it's a
    // managed account and whether sync is disabled by policies (which in some
    // cases involves fetching policies and can take a couple of seconds).
    host()->ShowScreen(contents(), GetSyncConfirmationURL(/*loading=*/true));

    ProfilePickerSignedInFlowController::Init();
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
    std::move(step_completed_callback_)
        .Run(std::move(post_host_cleared_callback), is_continue_callback,
             StepSwitchFinishedCallback());
  }

 private:
  IdentityStepsCompletedCallback step_completed_callback_;
};

}  // namespace

std::unique_ptr<ProfileManagementStepController> CreateIntroStep(
    ProfilePickerWebContentsHost* host,
    base::RepeatingCallback<void(IntroChoice)> choice_callback,
    bool enable_animations) {
  return std::make_unique<IntroStepController>(host, std::move(choice_callback),
                                               enable_animations);
}

FirstRunFlowControllerDice::FirstRunFlowControllerDice(
    ProfilePickerWebContentsHost* host,
    ClearHostClosure clear_host_callback,
    Profile* profile,
    ProfilePicker::FirstRunExitedCallback first_run_exited_callback)
    : ProfileManagementFlowControllerImpl(host, std::move(clear_host_callback)),
      profile_(profile),
      first_run_exited_callback_(std::move(first_run_exited_callback)) {
  DCHECK(profile_);
  DCHECK(first_run_exited_callback_);
}

FirstRunFlowControllerDice::~FirstRunFlowControllerDice() {
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

void FirstRunFlowControllerDice::Init(
    StepSwitchFinishedCallback step_switch_finished_callback) {
  RegisterStep(
      Step::kIntro,
      CreateIntroStep(host(),
                      base::BindRepeating(
                          &FirstRunFlowControllerDice::HandleIntroSigninChoice,
                          weak_ptr_factory_.GetWeakPtr()),
                      /*enable_animations=*/true));
  SwitchToStep(Step::kIntro, /*reset_state=*/true,
               std::move(step_switch_finished_callback));

  signin_metrics::LogSignInOffered(kAccessPoint);
}

void FirstRunFlowControllerDice::CancelPostSignInFlow() {
  // Called when the user declines enterprise management. Unfortunately, for
  // some technical and historical reasons, management is already marked as
  // accepted before we show the prompt. So here we need to revert it.
  // Currently we remove the account to match the behaviour from the profile
  // creation flow.
  // TODO(crbug.com/40067597): Refactor ProfilePickerSignedInFlowController
  // to split the lacros and dice behaviours more and remove the need for such
  // hacky workarounds. Look into letting the user keep their account.
  signin::ClearProfileWithManagedAccounts(profile_);

  HandleIdentityStepsCompleted(profile_, PostHostClearedCallback(),
                               /*is_continue_callback=*/false,
                               StepSwitchFinishedCallback());
}

bool FirstRunFlowControllerDice::PreFinishWithBrowser() {
  DCHECK(first_run_exited_callback_);
  std::move(first_run_exited_callback_)
      .Run(ProfilePicker::FirstRunExitStatus::kCompleted);
  return true;
}

void FirstRunFlowControllerDice::HandleIntroSigninChoice(IntroChoice choice) {
  if (choice == IntroChoice::kQuit) {
    // The view is getting destroyed. The class destructor will handle the rest.
    return;
  }

  if (choice == IntroChoice::kContinueWithoutAccount) {
    HandleIdentityStepsCompleted(profile_, PostHostClearedCallback(),
                                 /*is_continue_callback=*/false,
                                 StepSwitchFinishedCallback());
    return;
  }

  SwitchToIdentityStepsFromAccountSelection(
      /*step_switch_finished_callback=*/base::DoNothing(), kAccessPoint,
      profile_->GetPath());
}

std::unique_ptr<ProfilePickerSignedInFlowController>
FirstRunFlowControllerDice::CreateSignedInFlowController(
    Profile* signed_in_profile,
    const CoreAccountInfo& account_info,
    std::unique_ptr<content::WebContents> contents) {
  DCHECK_EQ(profile_, signed_in_profile);
  return std::make_unique<FirstRunPostSignInAdapter>(
      host(), signed_in_profile, account_info, std::move(contents),
      base::BindOnce(&FirstRunFlowControllerDice::HandleIdentityStepsCompleted,
                     // Unretained ok: the callback is passed to a step that
                     // the `this` will own and outlive.
                     base::Unretained(this), base::Unretained(profile_)));
}

void FirstRunFlowControllerDice::RunFinishFlowCallback() {
  if (finish_flow_callback_) {
    std::move(finish_flow_callback_).Run();
  }
}

base::queue<ProfileManagementFlowController::Step>
FirstRunFlowControllerDice::RegisterPostIdentitySteps(
    PostHostClearedCallback post_host_cleared_callback) {
  base::queue<ProfileManagementFlowController::Step> post_identity_steps;

  finish_flow_callback_ = base::BindOnce(
      &FirstRunFlowControllerDice::FinishFlowAndRunInBrowser,
      base::Unretained(this),
      // Unretained ok: the steps register a profile keep-alive and
      // will be alive until this callback runs.
      base::Unretained(profile_), std::move(post_host_cleared_callback));

  auto search_engine_choice_step_completed =
      base::BindOnce(&FirstRunFlowControllerDice::AdvanceToNextPostIdentityStep,
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
      base::BindOnce(&FirstRunFlowControllerDice::AdvanceToNextPostIdentityStep,
                     base::Unretained(this));
  RegisterStep(Step::kDefaultBrowser,
               std::make_unique<DefaultBrowserStepController>(
                   host(), std::move(default_browser_promo_step_completed)));
  post_identity_steps.emplace(
      ProfileManagementFlowController::Step::kDefaultBrowser);

  RegisterStep(
      Step::kFinishFlow,
      ProfileManagementStepController::CreateForFinishFlowAndRunInBrowser(
          host(),
          base::BindOnce(&FirstRunFlowControllerDice::RunFinishFlowCallback,
                         base::Unretained(this))));
  post_identity_steps.emplace(
      ProfileManagementFlowController::Step::kFinishFlow);
  return post_identity_steps;
}
