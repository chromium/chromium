// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"

#include "chrome/browser/ui/views/profiles/profile_picker_dice_sign_in_provider.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"

namespace {
class ProfilePickerAppStepController : public ProfileManagementStepController {
 public:
  explicit ProfilePickerAppStepController(ProfilePickerWebContentsHost* host,
                                          const GURL& initial_url)
      : ProfileManagementStepController(host), initial_url_(initial_url) {}

  ~ProfilePickerAppStepController() override = default;

  void Show(base::OnceCallback<void(bool)> step_shown_callback,
            bool reset_state) override {
    if (was_shown_) {
      if (reset_state) {
        // back to the beginning of the history:
        host()->GetPickerContents()->GetController().GoToIndex(0);
      }
      host()->ShowScreenInPickerContents(GURL());
    } else {
      host()->ShowScreenInPickerContents(initial_url_);
      was_shown_ = true;
    }
    if (step_shown_callback) {
      std::move(step_shown_callback).Run(true);
    }
  }

  void OnHidden() override {}

  void OnNavigateBackRequested() override {
    NavigateBackInternal(host()->GetPickerContents());
  }

 private:
  bool was_shown_ = false;
  GURL initial_url_;
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class DiceSignInStepController : public ProfileManagementStepController {
 public:
  explicit DiceSignInStepController(
      ProfilePickerWebContentsHost* host,
      std::unique_ptr<ProfilePickerDiceSignInProvider> dice_sign_in_provider,
      ProfilePickerDiceSignInProvider::SignedInCallback signed_in_callback)
      : ProfileManagementStepController(host),
        signed_in_callback_(std::move(signed_in_callback)),
        dice_sign_in_provider_(std::move(dice_sign_in_provider)) {
    DCHECK(signed_in_callback_);
  }

  ~DiceSignInStepController() override = default;

  void Show(base::OnceCallback<void(bool)> step_shown_callback,
            bool reset_state) override {
    DCHECK(step_shown_callback);
    DCHECK(!reset_state);  // Not supported.
    DCHECK(signed_in_callback_) << "Attempting to show Dice step again while "
                                   "it was previously completed";
    // Unretained ok because the provider is owned by `this`.
    dice_sign_in_provider_->SwitchToSignIn(
        std::move(step_shown_callback),
        base::BindOnce(&DiceSignInStepController::OnStepFinished,
                       base::Unretained(this)));
  }

  void OnHidden() override {
    host()->SetNativeToolbarVisible(false);
    // We don't reset the provider when we navigate back as we want to keep this
    // page and the ephemeral profile around for performance reasons.
    // The caller should delete the step if clearing the provider is needed.
  }

  bool CanPopStep() const override {
    return ProfileManagementStepController::CanPopStep() &&
           dice_sign_in_provider_ && dice_sign_in_provider_->IsInitialized();
  }

  void OnReloadRequested() override {
    // Sign-in may fail due to connectivity issues, allow reloading.
    if (dice_sign_in_provider_)
      dice_sign_in_provider_->ReloadSignInPage();
  }

  void OnNavigateBackRequested() override {
    if (dice_sign_in_provider_)
      NavigateBackInternal(dice_sign_in_provider_->contents());
  }

 private:
  void OnStepFinished(Profile* profile,
                      std::unique_ptr<content::WebContents> contents,
                      bool is_saml) {
    std::move(signed_in_callback_).Run(profile, std::move(contents), is_saml);
    // The step controller can be destroyed when `signed_in_callback_` runs.
    // Don't interact with members below.
  }

  ProfilePickerDiceSignInProvider::SignedInCallback signed_in_callback_;

  std::unique_ptr<ProfilePickerDiceSignInProvider> dice_sign_in_provider_;
};
#endif

class PostSignInStepController : public ProfileManagementStepController {
 public:
  explicit PostSignInStepController(
      ProfilePickerWebContentsHost* host,
      std::unique_ptr<ProfilePickerSignedInFlowController> signed_in_flow)
      : ProfileManagementStepController(host),
        signed_in_flow_(std::move(signed_in_flow)) {}

  ~PostSignInStepController() override = default;

  void Show(base::OnceCallback<void(bool)> step_shown_callback,
            bool reset_state) override {
    DCHECK(!reset_state);  // Not supported.
    signed_in_flow_->Init();
    if (step_shown_callback) {
      std::move(step_shown_callback).Run(true);
    }
  }
  void OnHidden() override { signed_in_flow_->Cancel(); }

  void OnNavigateBackRequested() override {
    // Do nothing, navigating back is not allowed.
  }

 private:
  std::unique_ptr<ProfilePickerSignedInFlowController> signed_in_flow_;
  base::WeakPtrFactory<PostSignInStepController> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<ProfileManagementStepController>
ProfileManagementStepController::CreateForProfilePickerApp(
    ProfilePickerWebContentsHost* host,
    const GURL& initial_url) {
  return std::make_unique<ProfilePickerAppStepController>(host, initial_url);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// static
std::unique_ptr<ProfileManagementStepController>
ProfileManagementStepController::CreateForDiceSignIn(
    ProfilePickerWebContentsHost* host,
    std::unique_ptr<ProfilePickerDiceSignInProvider> dice_sign_in_provider,
    base::OnceCallback<void(Profile* profile,
                            std::unique_ptr<content::WebContents>,
                            bool is_saml)> signed_in_callback) {
  return std::make_unique<DiceSignInStepController>(
      host, std::move(dice_sign_in_provider), std::move(signed_in_callback));
}
#endif

// static
std::unique_ptr<ProfileManagementStepController>
ProfileManagementStepController::CreateForPostSignInFlow(
    ProfilePickerWebContentsHost* host,
    std::unique_ptr<ProfilePickerSignedInFlowController> signed_in_flow) {
  return std::make_unique<PostSignInStepController>(host,
                                                    std::move(signed_in_flow));
}

ProfileManagementStepController::ProfileManagementStepController(
    ProfilePickerWebContentsHost* host)
    : host_(host) {}

ProfileManagementStepController::~ProfileManagementStepController() = default;

void ProfileManagementStepController::OnHidden() {}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProfileManagementStepController::OnReloadRequested() {}
#endif

void ProfileManagementStepController::NavigateBackInternal(
    content::WebContents* contents) {
  if (contents && contents->GetController().CanGoBack()) {
    contents->GetController().GoBack();
    return;
  }

  if (CanPopStep()) {
    DCHECK(pop_step_callback_);
    std::move(pop_step_callback_).Run();
  }
}

bool ProfileManagementStepController::CanPopStep() const {
  return !pop_step_callback_.is_null();
}
