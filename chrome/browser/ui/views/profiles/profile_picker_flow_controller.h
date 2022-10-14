// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FLOW_CONTROLLER_H_

#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"

class ProfilePickerSignedInFlowController;

class ProfilePickerFlowController : public ProfileManagementFlowController {
 public:
  ProfilePickerFlowController(ProfilePickerWebContentsHost* host,
                              ClearHostClosure clear_host_callback,
                              ProfilePicker::EntryPoint entry_point);
  ~ProfilePickerFlowController() override;

  void SwitchToDiceSignIn(
      absl::optional<SkColor> profile_color,
      base::OnceCallback<void(bool)> switch_finished_callback);

  void SwitchToPostSignIn(Profile* signed_in_profile,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
                          bool is_saml,
#endif
                          std::unique_ptr<content::WebContents> contents);

  // Cancel the signed-in profile setup and returns back to the main picker
  // screen (if the original EntryPoint was to open the picker).
  void CancelPostSignInFlow();

  base::FilePath GetSwitchProfilePathOrEmpty() const;

  void set_profile_color(absl::optional<SkColor> profile_color) {
    profile_color_ = profile_color;
  }

 private:
  ProfilePicker::EntryPoint entry_point_;
  absl::optional<SkColor> profile_color_ = absl::nullopt;

  // TODO(crbug.com/1359352): To be refactored out.
  // This is used for `ProfilePicker::GetSwitchProfilePath()`. The information
  // should ideally be provided to the handler of the profile switch page once
  // its controller is created instead of relying on static calls.
  base::WeakPtr<ProfilePickerSignedInFlowController>
      weak_signed_in_flow_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FLOW_CONTROLLER_H_
