// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/first_run_flow_controller_lacros.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_utils.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace {
// Registers a new `Observer` that will invoke `callback_` when `manager`
// notifies it via `OnRefreshTokensLoaded()`.
class OnRefreshTokensLoadedObserver : public signin::IdentityManager::Observer {
 public:
  OnRefreshTokensLoadedObserver(signin::IdentityManager* manager,
                                base::OnceClosure callback)
      : callback_(std::move(callback)) {
    DCHECK(callback_);
    identity_manager_observation_.Observe(manager);
  }

  // signin::IdentityManager::Observer
  void OnRefreshTokensLoaded() override {
    identity_manager_observation_.Reset();

    if (callback_) {
      std::move(callback_).Run();
    }
  }

 private:
  base::OnceClosure callback_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

class LacrosFirstRunSignedInFlowController
    : public ProfilePickerSignedInFlowController {
 public:
  // `finish_flow_callback` will be called when the user completes the FRE, but
  // might not be executed, for example if this object is destroyed before the
  // flow is completed.
  LacrosFirstRunSignedInFlowController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      std::unique_ptr<content::WebContents> contents,
      base::OnceClosure sync_confirmation_seen_callback,
      FinishFlowCallback finish_flow_callback)
      : ProfilePickerSignedInFlowController(
            host,
            profile,
            std::move(contents),
            signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE,
            absl::optional<SkColor>()),
        sync_confirmation_seen_callback_(
            std::move(sync_confirmation_seen_callback)),
        finish_flow_callback_(std::move(finish_flow_callback)) {}

  ~LacrosFirstRunSignedInFlowController() override = default;

  // ProfilePickerSignedInFlowController:
  void Init() override {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());

    if (can_retry_init_observer_) {
      can_retry_init_observer_.reset();
    }

    LOG(WARNING) << "Init running "
                 << (identity_manager->AreRefreshTokensLoaded() ? "with"
                                                                : "without")
                 << " refresh tokens.";

    if (!identity_manager->AreRefreshTokensLoaded()) {
      // We can't proceed with the init yet, as the tokens will be needed to
      // obtain extended account info and turn on sync. Register this method to
      // be called again when they become available.
      can_retry_init_observer_ =
          std::make_unique<OnRefreshTokensLoadedObserver>(
              identity_manager,
              base::BindOnce(
                  &LacrosFirstRunSignedInFlowController::Init,
                  // Unretained ok: `this` owns the observer and outlives it.
                  base::Unretained(this)));
      return;
    }

    ProfilePickerSignedInFlowController::Init();

    LOG(WARNING)
        << "Init completed and initiative handed off to TurnSyncOnHelper.";
  }

  void FinishAndOpenBrowser(PostHostClearedCallback callback) override {
    if (finish_flow_callback_.value()) {
      std::move(finish_flow_callback_.value()).Run(std::move(callback));
    }
  }

  void SwitchToEnterpriseProfileWelcome(
      EnterpriseProfileWelcomeUI::ScreenType type,
      signin::SigninChoiceCallback proceed_callback) override {
    if (!base::FeatureList::IsEnabled(kForYouFre)) {
      ProfilePickerSignedInFlowController::SwitchToEnterpriseProfileWelcome(
          type, std::move(proceed_callback));
      return;
    }

    host()->ShowScreen(
        contents(), GURL(chrome::kChromeUIIntroURL),
        base::BindOnce(
            &LacrosFirstRunSignedInFlowController::SwitchToIntroFinished,
            // Unretained ok: callback is called by the owner of this instance.
            base::Unretained(this), std::move(proceed_callback)));
  }

  void SwitchToSyncConfirmation() override {
    DCHECK(sync_confirmation_seen_callback_);  // Should be called only once.
    std::move(sync_confirmation_seen_callback_).Run();

    ProfilePickerSignedInFlowController::SwitchToSyncConfirmation();
  }

 private:
  void SwitchToIntroFinished(signin::SigninChoiceCallback proceed_callback) {
    base::OnceCallback signin_choice_adapter_callback =
        base::BindOnce([](IntroChoice choice) {
          switch (choice) {
            case IntroChoice::kContinueWithAccount:
              // Note: Indicates that the profile is "new" but will not result
              // in the creation of a new profile.
              return signin::SigninChoice::SIGNIN_CHOICE_NEW_PROFILE;
            case IntroChoice::kQuit:
              return signin::SigninChoice::SIGNIN_CHOICE_CANCEL;
          }
        });

    contents()
        ->GetWebUI()
        ->GetController()
        ->GetAs<IntroUI>()
        ->SetSigninChoiceCallback(
            IntroSigninChoiceCallback(std::move(signin_choice_adapter_callback)
                                          .Then(std::move(proceed_callback))));
  }

  // Callback that gets called when the user gets to the last step of the FRE.
  base::OnceClosure sync_confirmation_seen_callback_;

  // Callback that will be called when the user completes all the steps in the
  // flow, to finalize and close it.
  FinishFlowCallback finish_flow_callback_;

  std::unique_ptr<signin::IdentityManager::Observer> can_retry_init_observer_;
};

}  // namespace

FirstRunFlowControllerLacros::FirstRunFlowControllerLacros(
    ProfilePickerWebContentsHost* host,
    ClearHostClosure clear_host_callback,
    Profile* profile,
    ProfilePicker::FirstRunExitedCallback first_run_exited_callback)
    : ProfileManagementFlowControllerImpl(host, std::move(clear_host_callback)),
      profile_(profile),
      first_run_exited_callback_(std::move(first_run_exited_callback)) {
  DCHECK(first_run_exited_callback_);
}

FirstRunFlowControllerLacros::~FirstRunFlowControllerLacros() {
  // Call the callback if not called yet. This happens when the user exits the
  // flow by closing the window, or for intent overrides.
  if (first_run_exited_callback_) {
    std::move(first_run_exited_callback_)
        .Run(sync_confirmation_seen_
                 ? ProfilePicker::FirstRunExitStatus::kQuitAtEnd
                 : ProfilePicker::FirstRunExitStatus::kQuitEarly);
    // Since the flow is exited already, we don't have anything to close or
    // finish setting up.
  }
}

void FirstRunFlowControllerLacros::Init(
    StepSwitchFinishedCallback step_switch_finished_callback) {
  SwitchToIdentityStepsFromPostSignIn(
      profile_,
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_)),
      std::move(step_switch_finished_callback));
}

void FirstRunFlowControllerLacros::CancelPostSignInFlow() {
  NOTREACHED();  // The whole Lacros FRE is post-sign-in, it's not cancellable.
}

bool FirstRunFlowControllerLacros::PreFinishWithBrowser() {
  std::move(first_run_exited_callback_)
      .Run(ProfilePicker::FirstRunExitStatus::kCompleted);
  return true;
}

std::unique_ptr<ProfilePickerSignedInFlowController>
FirstRunFlowControllerLacros::CreateSignedInFlowController(
    Profile* signed_in_profile,
    std::unique_ptr<content::WebContents> contents,
    FinishFlowCallback finish_flow_callback) {
  auto mark_sync_confirmation_seen_callback =
      base::BindOnce(&FirstRunFlowControllerLacros::MarkSyncConfirmationSeen,
                     // Unretained ok: the callback is passed to a step that
                     // the `this` will own and outlive.
                     base::Unretained(this));

  auto signed_in_flow = std::make_unique<LacrosFirstRunSignedInFlowController>(
      host(), profile_, std::move(contents),
      std::move(mark_sync_confirmation_seen_callback),
      std::move(finish_flow_callback));
  return signed_in_flow;
}

void FirstRunFlowControllerLacros::MarkSyncConfirmationSeen() {
  sync_confirmation_seen_ = true;
}
