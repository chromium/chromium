// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/first_run_flow_controller_dice.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

void NavigateBackInOneSecond(
    base::WeakPtr<FirstRunFlowControllerDice> flow_controller) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FirstRunFlowControllerDice::OnNavigateBackRequested,
                     flow_controller),
      base::Milliseconds(1000));
}

class IntroStepController : public ProfileManagementStepController {
 public:
  explicit IntroStepController(
      ProfilePickerWebContentsHost* host,
      base::RepeatingCallback<void(bool sign_in)> choice_callback,
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
  const base::RepeatingCallback<void(bool sign_in)> choice_callback_;

  base::WeakPtrFactory<IntroStepController> weak_ptr_factory_{this};
};

// Placeholder step added to exercise the "back" behaviour.
// TODO(crbug.com/1375277): Replace with the real step for Dice sign in.
class PlaceholderStep : public ProfileManagementStepController {
 public:
  PlaceholderStep(ProfilePickerWebContentsHost* host,
                  GURL url,
                  std::unique_ptr<content::WebContents> contents)
      : ProfileManagementStepController(host),
        step_url_(url),
        contents_(std::move(contents)) {}
  void Show(base::OnceCallback<void(bool success)> step_shown_callback,
            bool reset_state) override {
    host()->ShowScreen(contents_.get(), step_url_,
                       base::BindOnce(std::move(step_shown_callback), true));
  }

  void OnNavigateBackRequested() override {
    NavigateBackInternal(contents_.get());
  }

 private:
  const GURL step_url_;
  const std::unique_ptr<content::WebContents> contents_;
};

}  // namespace

std::unique_ptr<ProfileManagementStepController> CreateIntroStep(
    ProfilePickerWebContentsHost* host,
    base::RepeatingCallback<void(bool sign_in)> choice_callback,
    bool enable_animations) {
  return std::make_unique<IntroStepController>(host, std::move(choice_callback),
                                               enable_animations);
}

FirstRunFlowControllerDice::FirstRunFlowControllerDice(
    ProfilePickerWebContentsHost* host,
    ClearHostClosure clear_host_callback,
    Profile* profile)
    : ProfileManagementFlowController(host,
                                      std::move(clear_host_callback),
                                      Step::kIntro),
      profile_(profile) {
  RegisterStep(
      initial_step(),
      CreateIntroStep(host,
                      base::BindRepeating(
                          &FirstRunFlowControllerDice::HandleIntroSigninChoice,
                          weak_ptr_factory_.GetWeakPtr()),
                      /*enable_animations=*/true));
}

FirstRunFlowControllerDice::~FirstRunFlowControllerDice() = default;

void FirstRunFlowControllerDice::HandleIntroSigninChoice(bool sign_in) {
  if (!sign_in) {
    FinishFlowAndRunInBrowser(profile_, PostHostClearedCallback());
    return;
  }

  RegisterStep(ProfileManagementFlowController::Step::kAccountSelection,
               std::make_unique<PlaceholderStep>(
                   host(), GURL(url::kAboutBlankURL),
                   content::WebContents::Create(
                       content::WebContents::CreateParams(profile_))));

  auto pop_closure = base::BindOnce(
      &ProfileManagementFlowController::SwitchToStep,
      // Binding as Unretained as `this` outlives the step
      // controllers.
      base::Unretained(this), ProfileManagementFlowController::Step::kIntro,
      /*reset_state=*/false, /*pop_step_callback=*/base::OnceClosure(),
      /*step_switch_finished_callback=*/base::OnceCallback<void(bool)>());
  SwitchToStep(ProfileManagementFlowController::Step::kAccountSelection,
               /*reset_state=*/true,
               /*pop_step_callback=*/std::move(pop_closure),
               /*step_switch_finished_callback=*/
               base::IgnoreArgs<bool>(base::BindOnce(
                   &NavigateBackInOneSecond, weak_ptr_factory_.GetWeakPtr())));
}
