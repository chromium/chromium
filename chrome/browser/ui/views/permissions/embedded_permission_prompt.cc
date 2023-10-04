// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt.h"

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_ask_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_policy_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_granted_view.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#endif

namespace {

EmbeddedPermissionPrompt::Variant HigherPriorityVariant(
    EmbeddedPermissionPrompt::Variant a,
    EmbeddedPermissionPrompt::Variant b) {
  return std::max(a, b);
}

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

bool IsPermissionSetByAdministator(ContentSetting setting,
                                   const content_settings::SettingInfo& info) {
  return ((setting == ContentSetting::CONTENT_SETTING_BLOCK ||
           setting == ContentSetting::CONTENT_SETTING_ALLOW) &&
          (info.source == content_settings::SETTING_SOURCE_POLICY ||
           info.source == content_settings::SETTING_SOURCE_SUPERVISED));
}

}  // namespace

EmbeddedPermissionPrompt::EmbeddedPermissionPrompt(
    Browser* browser,
    content::WebContents* web_contents,
    Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate),
      delegate_(delegate) {
  raw_ptr<HostContentSettingsMap> map =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  content_settings::SettingInfo info;

  embedded_prompt_variant_ = Variant::kUninitialized;
  for (const auto* request : delegate->Requests()) {
    ContentSettingsType type = request->GetContentSettingsType();
    ContentSetting setting =
        map->GetContentSetting(delegate->GetRequestingOrigin(),
                               delegate_->GetEmbeddingOrigin(), type, &info);
    Variant current_request_variant = DeterminePromptVariant(setting, info);
    embedded_prompt_variant_ = HigherPriorityVariant(embedded_prompt_variant_,
                                                     current_request_variant);
  }

  switch (embedded_prompt_variant_) {
    case Variant::kAsk:
      prompt_view_ =
          new EmbeddedPermissionPromptAskView(browser, delegate->GetWeakPtr());
      break;
    case Variant::kPreviouslyGranted:
      prompt_view_ = new EmbeddedPermissionPromptPreviouslyGrantedView(
          browser, delegate->GetWeakPtr());
      break;
    case Variant::kPreviouslyDenied:
    case Variant::kOsPrompt:
    case Variant::kOsSystemSettings:
      // TODO: Implement behavior for the above embedded prompt flavors.
      NOTREACHED();
      break;
    case Variant::kAdministratorGranted:
      prompt_view_ = new EmbeddedPermissionPromptPolicyView(
          browser, delegate->GetWeakPtr(), /*is_permission_allowed=*/true);
      break;
    case Variant::kAdministratorDenied:
      prompt_view_ = new EmbeddedPermissionPromptPolicyView(
          browser, delegate->GetWeakPtr(), /*is_permission_allowed=*/false);
      break;
    case Variant::kUninitialized:
      NOTREACHED();
  }

  if (prompt_view_) {
    prompt_view_->Show();
  }
}

EmbeddedPermissionPrompt::~EmbeddedPermissionPrompt() {
  if (prompt_view_) {
    prompt_view_->GetWidget()->Close();
  }
}

// static
EmbeddedPermissionPrompt::Variant
EmbeddedPermissionPrompt::DeterminePromptVariant(
    ContentSetting setting,
    const content_settings::SettingInfo& info) {
  if (IsPermissionSetByAdministator(setting, info)) {
    return setting == CONTENT_SETTING_ALLOW ? Variant::kAdministratorGranted
                                            : Variant::kAdministratorDenied;
  }

  switch (setting) {
    case CONTENT_SETTING_ASK:
      return Variant::kAsk;
    case CONTENT_SETTING_ALLOW:
      return Variant::kPreviouslyGranted;
    case CONTENT_SETTING_BLOCK:
      return Variant::kPreviouslyDenied;
    default:
      break;
  }

  return Variant::kUninitialized;
}

EmbeddedPermissionPrompt::TabSwitchingBehavior
EmbeddedPermissionPrompt::GetTabSwitchingBehavior() {
  return TabSwitchingBehavior::kKeepPromptAlive;
}

permissions::PermissionPromptDisposition
EmbeddedPermissionPrompt::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE;
}
