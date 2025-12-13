// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_post_sign_in_adapter.h"
#include "chrome/browser/ui/views/profiles/profile_picker_sign_in_provider.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"

namespace {

void ShowLoginErrorForBrowser(const SigninUIError& error, Browser* browser) {
  if (!browser) {
    // TODO(crbug.com/40242414): Make sure we do something or log an error if
    // opening a browser window was not possible.
    return;
  }
  LoginUIServiceFactory::GetForProfile(browser->profile())
      ->DisplayLoginResult(browser, error, /*from_profile_picker=*/false);
}

}  // namespace

ProfileManagementFlowControllerImpl::ProfileManagementFlowControllerImpl(
    ProfilePickerWebContentsHost* host,
    ClearHostClosure clear_host_callback,
    std::string_view flow_type_string)
    : ProfileManagementFlowController(host,
                                      std::move(clear_host_callback),
                                      flow_type_string) {}

ProfileManagementFlowControllerImpl::~ProfileManagementFlowControllerImpl() =
    default;

void ProfileManagementFlowControllerImpl::
    SwitchToIdentityStepsFromAccountSelection(
        StepSwitchFinishedCallback step_switch_finished_callback,
        signin_metrics::AccessPoint access_point,
        base::FilePath profile_path,
        const std::string& initial_email) {
  DCHECK_NE(Step::kAccountSelection, current_step());
  DCHECK_NE(Step::kPostSignInFlow, current_step());

  bool step_needs_registration = !IsStepInitialized(Step::kAccountSelection);
  if (step_needs_registration) {
    RegisterStep(
        Step::kAccountSelection,
        ProfileManagementStepController::CreateForSignIn(
            host(),
            std::make_unique<ProfilePickerSignInProvider>(
                host(), access_point, initial_email, std::move(profile_path)),
            base::BindOnce(
                &ProfileManagementFlowControllerImpl::HandleSignInCompleted,
                // Binding as Unretained as `this`
                // outlives the step controllers.
                base::Unretained(this)),
            base::BindOnce(
                &ProfileManagementFlowControllerImpl::HandleSigninError,
                base::Unretained(this))));
  }

  auto pop_back_step = current_step();
  if (current_step() == Step::kUnknown) {
    CHECK(!initial_email.empty());
    pop_back_step = Step::kProfilePicker;
  }
  SwitchToStep(Step::kAccountSelection,
               /*reset_state=*/step_needs_registration,
               std::move(step_switch_finished_callback),
               CreateSwitchToStepPopCallback(pop_back_step));
}

std::unique_ptr<ProfileManagementStepController>
ProfileManagementFlowControllerImpl::CreatePostSignInStep(
    Profile* signed_in_profile,
    const CoreAccountInfo& account_info,
    std::unique_ptr<content::WebContents> contents) {
  return ProfileManagementStepController::CreateForPostSignInFlow(
      host(), CreatePostSignInAdapter(signed_in_profile, account_info,
                                      std::move(contents)));
}

std::unique_ptr<ProfileManagementStepController>
ProfileManagementFlowControllerImpl::CreateSamlStep(
    Profile* signed_in_profile,
    std::unique_ptr<content::WebContents> contents) {
  return ProfileManagementStepController::CreateForFinishSamlSignIn(
      host(), signed_in_profile, std::move(contents),
      base::BindOnce(
          &ProfileManagementFlowControllerImpl::FinishFlowAndRunInBrowser,
          // Unretained ok: the callback is passed to a step that
          // the `flow_controller_` will own and outlive.
          base::Unretained(this),
          // Unretained ok: the steps register a profile alive and
          // will be alive until this callback runs.
          base::Unretained(signed_in_profile)));
}

void ProfileManagementFlowControllerImpl::HandleSignInCompleted(
    Profile* signed_in_profile,
    const CoreAccountInfo& account_info,
    std::unique_ptr<content::WebContents> contents,
    StepSwitchFinishedCallback step_switch_finished_callback) {
  DCHECK(signed_in_profile);
  DCHECK_EQ(Step::kAccountSelection, current_step());

  Step step;
  // SAML with ForceSignin flow is migrated to the regular Profile Picker flow.
  if (account_info.IsEmpty() && !signin_util::IsForceSigninEnabled()) {
    step = Step::kFinishSamlSignin;
    DCHECK(!IsStepInitialized(step));
    // The SAML step controller handles finishing the profile setup by itself
    // when we switch to it.
    RegisterStep(step, CreateSamlStep(signed_in_profile, std::move(contents)));
  } else {
    step = Step::kPostSignInFlow;
    DCHECK(!IsStepInitialized(step));
    RegisterStep(step, CreatePostSignInStep(signed_in_profile, account_info,
                                            std::move(contents)));
  }

  SwitchToStep(step, /*reset_state=*/true,
               std::move(step_switch_finished_callback));

  // If we need to go back, we should go all the way to the beginning of the
  // flow and after that, recreate the account selection step to ensure no data
  // leaks if we select a different account.
  // We also erase the step after the switch here because it holds a
  // `ScopedProfileKeepAlive` and we need the next step to register its own
  // before this the account selection's is released.
  UnregisterStep(Step::kAccountSelection);
}

void ProfileManagementFlowControllerImpl::HandleSigninError(
    Profile* profile,
    content::WebContents* contents,
    const SigninUIError& error) {
  CHECK_EQ(Step::kAccountSelection, current_step());
  CHECK(!error.IsOk());

  RegisterStep(
      Step::kFinishFlow,
      ProfileManagementStepController::CreateForFinishFlowAndRunInBrowser(
          host(),
          base::BindOnce(
              &ProfileManagementFlowControllerImpl::FinishFlowAndRunInBrowser,
              base::Unretained(this), profile,
              PostHostClearedCallback(
                  base::BindOnce(&ShowLoginErrorForBrowser,
                                 error)))));
  SwitchToStep(Step::kFinishFlow, /*reset_state=*/true);
  return;
}

void ProfileManagementFlowControllerImpl::SwitchToPostIdentitySteps(
    PostHostClearedCallback post_host_cleared_callback) {
  post_identity_steps_ =
      RegisterPostIdentitySteps(std::move(post_host_cleared_callback));
  AdvanceToNextPostIdentityStep();
}

void ProfileManagementFlowControllerImpl::AdvanceToNextPostIdentityStep() {
  if (post_identity_steps_.empty()) {
    return;
  }

  Step next_step = post_identity_steps_.front();
  post_identity_steps_.pop();
  SwitchToStep(next_step, /*reset_state=*/true);
}

void ProfileManagementFlowControllerImpl::HandleIdentityStepsCompleted(
    Profile* profile,
    PostHostClearedCallback post_host_cleared_callback,
    bool is_continue_callback) {
  CHECK(profile);

  if (is_continue_callback) {
    RegisterStep(
        Step::kFinishFlow,
        ProfileManagementStepController::CreateForFinishFlowAndRunInBrowser(
            host(),
            base::BindOnce(
                &ProfileManagementFlowControllerImpl::FinishFlowAndRunInBrowser,
                base::Unretained(this), profile,
                std::move(post_host_cleared_callback))));
    SwitchToStep(Step::kFinishFlow, /*reset_state=*/true);
    return;
  }

  SwitchToPostIdentitySteps(std::move(post_host_cleared_callback));
}
