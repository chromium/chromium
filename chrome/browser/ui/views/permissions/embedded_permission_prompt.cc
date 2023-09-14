// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt.h"

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_policy_view.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#endif

namespace {
#if BUILDFLAG(IS_MAC)
void OpenCameraPermissionSystemSettingsMacOS() {
  if (system_media_permissions::CheckSystemVideoCapturePermission() ==
      system_media_permissions::SystemPermission::kDenied) {
    base::mac::OpenSystemSettingsPane(
        base::mac::SystemSettingsPane::kPrivacySecurity_Camera);
  }
}

void OpenMicrophonePermissionSystemSettingsMacOS() {
  if (system_media_permissions::CheckSystemAudioCapturePermission() ==
      system_media_permissions::SystemPermission::kDenied) {
    base::mac::OpenSystemSettingsPane(
        base::mac::SystemSettingsPane::kPrivacySecurity_Microphone);
  }
}
#endif
}  // namespace

EmbeddedPermissionPrompt::EmbeddedPermissionPrompt(
    Browser* browser,
    content::WebContents* web_contents,
    Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate) {
  // TODO(crbug.com/1462930): pick the correct EmbeddedPermissionPromptBaseView
  // subclass into |prompt_view_| and `Show()` it.
}

EmbeddedPermissionPrompt::~EmbeddedPermissionPrompt() = default;

EmbeddedPermissionPrompt::TabSwitchingBehavior
EmbeddedPermissionPrompt::GetTabSwitchingBehavior() {
  return TabSwitchingBehavior::kKeepPromptAlive;
}

permissions::PermissionPromptDisposition
EmbeddedPermissionPrompt::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE;
}
