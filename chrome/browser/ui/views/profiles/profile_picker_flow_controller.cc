// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_flow_controller.h"

#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/views/profiles/profile_creation_signed_in_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/common/webui_url_constants.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/views/profiles/profile_picker_dice_sign_in_provider.h"
#endif

namespace {
// Returns the URL to load as initial content for the profile picker. If an
// empty URL is returned, the profile picker should not be shown until
// another explicit call with a non-empty URL given to the view
// (see `ProfilePickerView::ShowScreen()` for example)
GURL GetInitialURL(ProfilePicker::EntryPoint entry_point) {
  GURL base_url = GURL(chrome::kChromeUIProfilePickerUrl);
  switch (entry_point) {
    case ProfilePicker::EntryPoint::kOnStartup: {
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
      return base_url;
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile:
      return base_url.Resolve("new-profile");
    case ProfilePicker::EntryPoint::kLacrosSelectAvailableAccount:
      return base_url.Resolve("account-selection-lacros");
    case ProfilePicker::EntryPoint::kLacrosPrimaryProfileFirstRun:
      // Should not be used for this entry point.
      NOTREACHED();
      return GURL();
  }
}
}  // namespace

ProfilePickerFlowController::ProfilePickerFlowController(
    ProfilePickerWebContentsHost* host,
    ProfilePicker::EntryPoint entry_point)
    : ProfileManagementFlowController(host, Step::kProfilePicker),
      entry_point_(entry_point) {
  RegisterStep(initial_step(),
               ProfileManagementStepController::CreateForProfilePickerApp(
                   host, GetInitialURL(entry_point_)));
}

ProfilePickerFlowController::~ProfilePickerFlowController() = default;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProfilePickerFlowController::SwitchToDiceSignIn(
    absl::optional<SkColor> profile_color,
    base::OnceCallback<void(bool)> switch_finished_callback) {
  DCHECK_EQ(Step::kProfilePicker, current_step());

  profile_color_ = profile_color;
  if (!IsStepInitialized(Step::kAccountSelection)) {
    RegisterStep(
        Step::kAccountSelection,
        ProfileManagementStepController::CreateForDiceSignIn(
            host(), std::make_unique<ProfilePickerDiceSignInProvider>(host()),
            base::BindOnce(&ProfilePickerFlowController::SwitchToPostSignIn,
                           // Binding as Unretained as `this` outlives the step
                           // controllers.
                           base::Unretained(this))));
  }
  auto pop_closure = base::BindOnce(
      &ProfilePickerFlowController::SwitchToStep,
      // Binding as Unretained as `this` outlives the step
      // controllers.
      base::Unretained(this), Step::kProfilePicker,
      /*reset_state=*/false, /*pop_step_callback=*/base::OnceClosure(),
      /*step_switch_finished_callback=*/base::OnceCallback<void(bool)>());
  SwitchToStep(Step::kAccountSelection,
               /*reset_state=*/false, std::move(pop_closure),
               std::move(switch_finished_callback));
}
#endif

void ProfilePickerFlowController::SwitchToPostSignIn(
    Profile* signed_in_profile,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    bool is_saml,
#endif
    std::unique_ptr<content::WebContents> contents) {
  DCHECK(!signin_util::IsForceSigninEnabled());
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  DCHECK_EQ(Step::kAccountSelection, current_step());
#endif
  DCHECK(signed_in_profile);

  DCHECK(!IsStepInitialized(Step::kPostSignInFlow));

  // TODO(crbug.com/1360055): Split out the SAML flow directly from here instead
  // of using `ProfileCreationSignedInFlowController` for it.
  auto signed_in_flow = std::make_unique<ProfileCreationSignedInFlowController>(
      host(), signed_in_profile, std::move(contents), profile_color_,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      is_saml
#else
      false
#endif
  );

  weak_signed_in_flow_controller_ = signed_in_flow->GetWeakPtr();
  RegisterStep(Step::kPostSignInFlow,
               ProfileManagementStepController::CreateForPostSignInFlow(
                   host(), std::move(signed_in_flow)));

  SwitchToStep(Step::kPostSignInFlow);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // If we need to go back, we should go all the way to the beginning of the
  // flow and after that, recreate the account selection step to ensure no data
  // leaks if we select a different account.
  // We also erase the step after the switch here because it holds a
  // `ScopedProfileKeepAlive` and we need the next step to register its own
  // before this the account selection's is released.
  UnregisterStep(Step::kAccountSelection);
#endif
}

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
    case ProfilePicker::EntryPoint::kOnStartup:
    case ProfilePicker::EntryPoint::kProfileMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcess:
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
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile: {
      // This results in destroying `this`.
      host()->Clear();
      return;
    }
    case ProfilePicker::EntryPoint::kLacrosSelectAvailableAccount:
    case ProfilePicker::EntryPoint::kLacrosPrimaryProfileFirstRun:
      NOTREACHED()
          << "CancelPostSignInFlow() is not reachable from this entry point";
      return;
  }
}
