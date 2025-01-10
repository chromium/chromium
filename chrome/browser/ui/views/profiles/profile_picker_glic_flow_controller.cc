// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_glic_flow_controller.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/common/webui_url_constants.h"

namespace {

GURL GetProfilePickerGlicURL() {
  const GURL base_url = GURL(chrome::kChromeUIProfilePickerUrl);
  GURL::Replacements replacements;
  replacements.SetQueryStr(chrome::kChromeUIProfilePickerGlicQuery);
  return base_url.ReplaceComponents(replacements);
}

}  // namespace

ProfilePickerGlicFlowController::ProfilePickerGlicFlowController(
    ProfilePickerWebContentsHost* host,
    ClearHostClosure clear_host_callback,
    base::OnceCallback<void(Profile*)> picked_profile_callback)
    : ProfileManagementFlowController(host, std::move(clear_host_callback)),
      picked_profile_callback_(std::move(picked_profile_callback)) {
  CHECK(picked_profile_callback_);
}

ProfilePickerGlicFlowController::~ProfilePickerGlicFlowController() {
  if (picked_profile_callback_) {
    Clear();
  }
}

void ProfilePickerGlicFlowController::Init(
    StepSwitchFinishedCallback step_switch_finished_callback) {
  RegisterStep(Step::kProfilePicker,
               ProfileManagementStepController::CreateForProfilePickerApp(
                   host(), GetProfilePickerGlicURL()));
  SwitchToStep(Step::kProfilePicker, /*reset_state=*/true,
               std::move(step_switch_finished_callback));
}

void ProfilePickerGlicFlowController::PickProfile(
    const base::FilePath& profile_path,
    ProfilePicker::ProfilePickingArgs args) {
  g_browser_process->profile_manager()->LoadProfileByPath(
      profile_path, /*incognito=*/false,
      base::BindOnce(&ProfilePickerGlicFlowController::OnPickedProfileLoaded,
                     base::Unretained(this)));
}

void ProfilePickerGlicFlowController::OnPickedProfileLoaded(Profile* profile) {
  if (!profile) {
    Clear();
    return;
  }

  // TODO(crbug.com/388211126): Add a new specific keep alive in order not to
  // rely on `kWaitingForFirstBrowserWindow` that is not accurate and has
  // special conditions.

  // Return the loaded `profile` to the caller.
  std::move(picked_profile_callback_).Run(profile);

  // Close the picker.
  ExitFlow();
}

void ProfilePickerGlicFlowController::Clear() {
  std::move(picked_profile_callback_).Run(nullptr);
  ExitFlow();
}

void ProfilePickerGlicFlowController::CancelPostSignInFlow() {
  NOTREACHED() << "The glic flow controller is not expected to support this "
                  "part of the flow as it does not support signing in.";
}
