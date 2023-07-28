// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/first_run_flow_controller_dice.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

namespace {

const signin_metrics::AccessPoint kAccessPoint =
    signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE;

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
      base::OnceClosure step_completed_callback)
      : ProfileManagementStepController(host),
        step_completed_callback_(std::move(step_completed_callback)) {}

  ~DefaultBrowserStepController() override = default;

  void Show(base::OnceCallback<void(bool success)> step_shown_callback,
            bool reset_state) override {
    CHECK(reset_state);

    base::OnceClosure navigation_finished_closure = base::BindOnce(
        &DefaultBrowserStepController::OnLoadFinished, base::Unretained(this));
    if (step_shown_callback) {
      // Notify the caller first.
      navigation_finished_closure =
          base::BindOnce(std::move(step_shown_callback), true)
              .Then(std::move(navigation_finished_closure));
    }

    host()->ShowScreenInPickerContents(
        // TODO(crbug.com/1465822): Implement the WebUI page and wait for user
        // response.
        GURL(chrome::kChromeUIIntroDefaultBrowserURL),
        std::move(navigation_finished_closure));
  }

  void OnNavigateBackRequested() override {
    // Do nothing, navigating back is not allowed.
  }

 private:
  void OnLoadFinished() {
    // TODO(crbug.com/1465822): Configure WebUI controller and wait for a user
    // choice to advance. For now we auto-advance as a placeholder.
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DefaultBrowserStepController::OnStepCompleted,
                       // WeakPtr: Because of the delayed task.
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(2));
  }

  void OnStepCompleted() {
    CHECK(step_completed_callback_);
    std::move(step_completed_callback_).Run();
  }

  // Callback to be executed when the step is completed.
  base::OnceClosure step_completed_callback_;

  base::WeakPtrFactory<DefaultBrowserStepController> weak_ptr_factory_{this};
};

using IdentityStepsCompletedCallback =
    base::OnceCallback<void(PostHostClearedCallback post_host_cleared_callback,
                            bool is_continue_callback)>;

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
      std::unique_ptr<content::WebContents> contents,
      IdentityStepsCompletedCallback step_completed_callback)
      : ProfilePickerSignedInFlowController(host,
                                            profile,
                                            std::move(contents),
                                            kAccessPoint,
                                            /*profile_color=*/absl::nullopt),
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

  void FinishAndOpenBrowser(
      PostHostClearedCallback post_host_cleared_callback) override {
    // Do nothing if this has already been called. Note that this can get called
    // first time from a special case handling (such as the Settings link) and
    // than second time when the TurnSyncOnHelper finishes.
    if (!step_completed_callback_) {
      return;
    }

    // The only callback we can receive in this flow is the one to
    // finish configuring Sync. In this case we always want to
    // immediately continue with that.
    bool is_continue_callback = !post_host_cleared_callback->is_null();
    std::move(step_completed_callback_)
        .Run(std::move(post_host_cleared_callback), is_continue_callback);
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
  bool is_core_flow_completed = current_step() == Step::kDefaultBrowser;

  if (is_core_flow_completed) {
    FinishFlowAndRunInBrowser(profile_, std::move(post_host_cleared_callback_));
  } else {
    // TODO(crbug.com/1466803): Revisit the enum value name for kQuitAtEnd.
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
  // TODO(crbug.com/1465779): If on enterprise profile welcome, sign the user
  // out and continue with a local profile. Probably would just consist in
  // aborting the TSOH's flow, which should remove the account. Maybe we'd need
  // to advance to a separate step to allow deleting all the objects and getting
  // the account fully removed before opening the browser?
  NOTIMPLEMENTED();

  HandleIdentityStepsCompleted(PostHostClearedCallback());
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
    HandleIdentityStepsCompleted(PostHostClearedCallback());
    return;
  }

  SwitchToIdentityStepsFromAccountSelection(
      /*step_switch_finished_callback=*/base::DoNothing());
}

void FirstRunFlowControllerDice::HandleIdentityStepsCompleted(
    PostHostClearedCallback post_host_cleared_callback,
    bool is_continue_callback) {
  CHECK(post_host_cleared_callback_->is_null());

  post_host_cleared_callback_ = std::move(post_host_cleared_callback);

  // TODO(crbug.com/1465822): Also check policy and the current default state.
  bool should_show_default_browser_step =
      // Proceed with the callback  directly instead of showing the default
      // browser prompt.
      !is_continue_callback &&
      // The feature configuration ultimately gates the step.
      kForYouFreWithDefaultBrowserStep.Get() != WithDefaultBrowserStep::kNo;

  if (!should_show_default_browser_step) {
    FinishFlowAndRunInBrowser(profile_, std::move(post_host_cleared_callback_));
    return;
  }

  auto step_finished_callback =
      base::BindOnce(&FirstRunFlowControllerDice::FinishFlowAndRunInBrowser,
                     // Unretained ok: the step is owned by `this`.
                     base::Unretained(this), base::Unretained(profile_),
                     std::move(post_host_cleared_callback_));
  RegisterStep(Step::kDefaultBrowser,
               std::make_unique<DefaultBrowserStepController>(
                   host(), std::move(step_finished_callback)));
  SwitchToStep(Step::kDefaultBrowser, /*reset_state=*/true);
}

std::unique_ptr<ProfilePickerDiceSignInProvider>
FirstRunFlowControllerDice::CreateDiceSignInProvider() {
  return std::make_unique<ProfilePickerDiceSignInProvider>(host(), kAccessPoint,
                                                           profile_->GetPath());
}

std::unique_ptr<ProfilePickerSignedInFlowController>
FirstRunFlowControllerDice::CreateSignedInFlowController(
    Profile* signed_in_profile,
    std::unique_ptr<content::WebContents> contents) {
  DCHECK_EQ(profile_, signed_in_profile);
  return std::make_unique<FirstRunPostSignInAdapter>(
      host(), signed_in_profile, std::move(contents),
      base::BindOnce(&FirstRunFlowControllerDice::HandleIdentityStepsCompleted,
                     // Unretained ok: the callback is passed to a step that
                     // the `this` will own and outlive.
                     base::Unretained(this)));
}
