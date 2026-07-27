// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_omnibox_everywhere_flow_controller.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/common/webui_url_constants.h"

namespace {

GURL GetProfilePickerOmniboxEverywhereURL() {
  const GURL base_url = GURL(chrome::kChromeUIProfilePickerUrl);
  GURL::Replacements replacements;
  replacements.SetQueryStr(
      chrome::kChromeUIProfilePickerOmniboxEverywhereQuery);
  return base_url.ReplaceComponents(replacements);
}

}  // namespace

ProfilePickerOmniboxEverywhereFlowController::
    ProfilePickerOmniboxEverywhereFlowController(
        ProfilePickerWebContentsHost* host,
        ClearHostClosure clear_host_callback,
        base::OnceCallback<void(Profile*)> picked_profile_callback)
    : ProfileManagementFlowController(
          host,
          std::move(clear_host_callback),
          /*flow_type_string=*/"OmniboxEverywhereFlow"),
      picked_profile_callback_(std::move(picked_profile_callback)) {
  CHECK(picked_profile_callback_);
}

ProfilePickerOmniboxEverywhereFlowController::
    ~ProfilePickerOmniboxEverywhereFlowController() {
  if (picked_profile_callback_) {
    std::move(picked_profile_callback_).Run(nullptr);
  }
}

void ProfilePickerOmniboxEverywhereFlowController::Init() {
  RegisterStep(Step::kProfilePicker,
               ProfileManagementStepController::CreateForProfilePickerApp(
                   host(), GetProfilePickerOmniboxEverywhereURL()));
  SwitchToStep(Step::kProfilePicker, /*reset_state=*/true);
}

void ProfilePickerOmniboxEverywhereFlowController::PickProfile(
    const base::FilePath& profile_path,
    ProfilePicker::ProfilePickingArgs args,
    base::OnceCallback<void(bool)> pick_profile_complete_callback) {
  g_browser_process->profile_manager()->LoadProfileByPath(
      profile_path, /*incognito=*/false,
      base::BindOnce(
          &ProfilePickerOmniboxEverywhereFlowController::OnPickedProfileLoaded,
          weak_ptr_factory_.GetWeakPtr(),
          std::move(pick_profile_complete_callback)));
}

void ProfilePickerOmniboxEverywhereFlowController::OnPickedProfileLoaded(
    base::OnceCallback<void(bool)> pick_profile_complete_callback,
    Profile* profile) {
  if (pick_profile_complete_callback) {
    std::move(pick_profile_complete_callback).Run(profile != nullptr);
  }

  if (picked_profile_callback_) {
    std::move(picked_profile_callback_).Run(profile);
  }

  ExitFlow();
}

void ProfilePickerOmniboxEverywhereFlowController::Clear() {
  if (picked_profile_callback_) {
    std::move(picked_profile_callback_).Run(nullptr);
  }
  ExitFlow();
}

void ProfilePickerOmniboxEverywhereFlowController::CancelSigninFlow() {
  NOTREACHED() << "The Omnibox Everywhere flow controller is not expected to "
                  "support signin flow.";
}
