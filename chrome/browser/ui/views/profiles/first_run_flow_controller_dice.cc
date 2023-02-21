// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/first_run_flow_controller_dice.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_utils.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_metrics.h"
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

// Instance allowing `TurnSyncOnHelper` to  drive the interface in the
// `kPostSignIn` step.
//
// Not following the `*SignedInFlowController` naming pattern to avoid confusion
// with `*StepController` and `*FlowController` that we also have here.
// `ProfilePickerSignedInFlowController` should eventually be renamed.
class FirstRunPostSignInAdapter : public ProfilePickerSignedInFlowController {
 public:
  FirstRunPostSignInAdapter(ProfilePickerWebContentsHost* host,
                            Profile* profile,
                            std::unique_ptr<content::WebContents> contents,
                            FinishFlowCallback finish_flow_callback)
      : ProfilePickerSignedInFlowController(host,
                                            profile,
                                            std::move(contents),
                                            kAccessPoint,
                                            /*profile_color=*/absl::nullopt),
        finish_flow_callback_(std::move(finish_flow_callback)) {
    DCHECK(finish_flow_callback_.value());
  }

  void Init() override {
    // Stop with the sign-in navigation and show a spinner instead. The spinner
    // will be shown until TurnSyncOnHelper figures out whether it's a
    // managed account and whether sync is disabled by policies (which in some
    // cases involves fetching policies and can take a couple of seconds).
    host()->ShowScreen(contents(), GetSyncConfirmationURL(/*loading=*/true));

    ProfilePickerSignedInFlowController::Init();
  }

  void FinishAndOpenBrowser(PostHostClearedCallback callback) override {
    // Do nothing if this has already been called. Note that this can get called
    // first time from a special case handling (such as the Settings link) and
    // than second time when the TurnSyncOnHelper finishes.
    if (finish_flow_callback_->is_null())
      return;

    std::move(finish_flow_callback_.value()).Run(std::move(callback));
  }

 private:
  FinishFlowCallback finish_flow_callback_;
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
  if (first_run_exited_callback_) {
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
  // TODO(crbug.com/1375277): If on enterprise profile welcome, sign the user
  // out and continue with a local profile. Probably would just consist in
  // aborting the TSOH's flow, which should remove the account. Maybe we'd need
  // to advance to a separate step to allow deleting all the objects and getting
  // the account fully removed before opening the browser?
  NOTIMPLEMENTED();

  FinishFlowAndRunInBrowser(profile_, PostHostClearedCallback());
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
    FinishFlowAndRunInBrowser(profile_, PostHostClearedCallback());
    return;
  }

  SwitchToIdentityStepsFromAccountSelection(
      /*step_switch_finished_callback=*/base::DoNothing());
}

std::unique_ptr<ProfilePickerDiceSignInProvider>
FirstRunFlowControllerDice::CreateDiceSignInProvider() {
  return std::make_unique<ProfilePickerDiceSignInProvider>(host(), kAccessPoint,
                                                           profile_->GetPath());
}

std::unique_ptr<ProfilePickerSignedInFlowController>
FirstRunFlowControllerDice::CreateSignedInFlowController(
    Profile* signed_in_profile,
    std::unique_ptr<content::WebContents> contents,
    FinishFlowCallback finish_flow_callback) {
  DCHECK_EQ(profile_, signed_in_profile);
  return std::make_unique<FirstRunPostSignInAdapter>(
      host(), signed_in_profile, std::move(contents),
      std::move(finish_flow_callback));
}
