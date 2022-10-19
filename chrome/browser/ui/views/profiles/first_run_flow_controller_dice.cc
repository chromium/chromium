// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/first_run_flow_controller_dice.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/common/webui_url_constants.h"
#include "url/gurl.h"

namespace {

class IntroStepController : public ProfileManagementStepController {
 public:
  explicit IntroStepController(ProfilePickerWebContentsHost* host,
                               bool enable_animations)
      : ProfileManagementStepController(host) {
    std::string url_string = chrome::kChromeUIIntroURL;
    if (!enable_animations) {
      url_string += "?noAnimations";
    }
    intro_url_ = GURL(url_string);
  }

  ~IntroStepController() override = default;

  void Show(base::OnceCallback<void(bool success)> step_shown_callback,
            bool reset_state) override {
    DCHECK(!reset_state);  // Not supported

    host()->ShowScreenInPickerContents(
        intro_url_, base::BindOnce(&IntroStepController::OnIntroLoaded,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(step_shown_callback)));
  }

  void OnHidden() override {}

  void OnNavigateBackRequested() override {}

  void OnIntroLoaded(base::OnceCallback<void(bool)> step_shown_callback) {
    if (step_shown_callback) {
      std::move(step_shown_callback).Run(/*success=*/true);
    }
  }

 private:
  GURL intro_url_;

  base::WeakPtrFactory<IntroStepController> weak_ptr_factory_{this};
};

}  // namespace

std::unique_ptr<ProfileManagementStepController> CreateIntroStep(
    ProfilePickerWebContentsHost* host,
    bool enable_animations) {
  return std::make_unique<IntroStepController>(host, enable_animations);
}

FirstRunFlowControllerDice::FirstRunFlowControllerDice(
    ProfilePickerWebContentsHost* host,
    ClearHostClosure clear_host_callback)
    : ProfileManagementFlowController(host,
                                      std::move(clear_host_callback),
                                      Step::kIntro) {
  RegisterStep(initial_step(),
               CreateIntroStep(host, /*enable_animations=*/true));
}
FirstRunFlowControllerDice::~FirstRunFlowControllerDice() = default;
