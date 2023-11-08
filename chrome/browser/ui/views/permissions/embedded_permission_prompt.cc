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
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_denied_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_granted_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_show_system_prompt_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_system_settings_view.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#endif

namespace {

bool CanGroupVariants(EmbeddedPermissionPrompt::Variant a,
                      EmbeddedPermissionPrompt::Variant b) {
  // Ask and PreviouslyDenied are a special case and can be grouped together.
  if ((a == EmbeddedPermissionPrompt::Variant::kPreviouslyDenied &&
       b == EmbeddedPermissionPrompt::Variant::kAsk) ||
      (a == EmbeddedPermissionPrompt::Variant::kAsk &&
       b == EmbeddedPermissionPrompt::Variant::kPreviouslyDenied)) {
    return true;
  }

  return (a == b);
}

bool IsPermissionSetByAdministator(ContentSetting setting,
                                   const content_settings::SettingInfo& info) {
  return ((setting == ContentSetting::CONTENT_SETTING_BLOCK ||
           setting == ContentSetting::CONTENT_SETTING_ALLOW) &&
          (info.source == content_settings::SETTING_SOURCE_POLICY ||
           info.source == content_settings::SETTING_SOURCE_SUPERVISED));
}

#if BUILDFLAG(IS_MAC)
void OpenCameraSystemSettingsOnMacOS() {
  if (system_media_permissions::CheckSystemVideoCapturePermission() ==
      system_media_permissions::SystemPermission::kDenied) {
    base::mac::OpenSystemSettingsPane(
        base::mac::SystemSettingsPane::kPrivacySecurity_Camera);
  }
}

void OpenMicSystemSettingsOnMacOS() {
  if (system_media_permissions::CheckSystemAudioCapturePermission() ==
      system_media_permissions::SystemPermission::kDenied) {
    base::mac::OpenSystemSettingsPane(
        base::mac::SystemSettingsPane::kPrivacySecurity_Microphone);
  }
}

bool ShouldShowSystemSettingsViewOnMacOS(ContentSettingsType type) {
  return (type == ContentSettingsType::MEDIASTREAM_MIC &&
          system_media_permissions::CheckSystemAudioCapturePermission() ==
              system_media_permissions::SystemPermission::kDenied) ||
         (type == ContentSettingsType::MEDIASTREAM_CAMERA &&
          system_media_permissions::CheckSystemVideoCapturePermission() ==
              system_media_permissions::SystemPermission::kDenied);
}

bool ShouldShowOSPromptViewOnMacOS(ContentSettingsType type) {
  return (type == ContentSettingsType::MEDIASTREAM_MIC &&
          system_media_permissions::CheckSystemAudioCapturePermission() ==
              system_media_permissions::SystemPermission::kNotDetermined) ||
         (type == ContentSettingsType::MEDIASTREAM_CAMERA &&
          system_media_permissions::CheckSystemVideoCapturePermission() ==
              system_media_permissions::SystemPermission::kNotDetermined);
}
#endif

}  // namespace

EmbeddedPermissionPrompt::EmbeddedPermissionPrompt(
    Browser* browser,
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate),
      delegate_(delegate) {
  CloseCurrentViewAndMaybeShowNext(/*first_prompt=*/true);
}

EmbeddedPermissionPrompt::~EmbeddedPermissionPrompt() {
  CloseView();
}

base::WeakPtr<permissions::PermissionPrompt::Delegate>
EmbeddedPermissionPrompt::GetPermissionPromptDelegate() const {
  return delegate_->GetWeakPtr();
}

const std::vector<permissions::PermissionRequest*>&
EmbeddedPermissionPrompt::Requests() const {
  return requests_;
}

// static
EmbeddedPermissionPrompt::Variant
EmbeddedPermissionPrompt::DeterminePromptVariant(
    ContentSetting setting,
    const content_settings::SettingInfo& info,
    ContentSettingsType type) {
  // First determine if we can directly show one of the OS views, if the
  // permission was granted (previously or by Administrator).
  if (setting == CONTENT_SETTING_ALLOW) {
    // TODO(crbug.com/1462930): Handle going to Windows settings.
#if BUILDFLAG(IS_MAC)
    if (ShouldShowSystemSettingsViewOnMacOS(type)) {
      return Variant::kOsSystemSettings;
    }

    if (ShouldShowOSPromptViewOnMacOS(type)) {
      return Variant::kOsPrompt;
    }
#endif
  }

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

void EmbeddedPermissionPrompt::CloseCurrentViewAndMaybeShowNext(
    bool first_prompt) {
  if (!first_prompt) {
    CloseView();
  }

  auto* map = HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  content_settings::SettingInfo info;

  for (const auto* request : delegate()->Requests()) {
    ContentSettingsType type = request->GetContentSettingsType();
    ContentSetting setting =
        map->GetContentSetting(delegate()->GetRequestingOrigin(),
                               delegate()->GetEmbeddingOrigin(), type, &info);
    Variant current_request_variant =
        DeterminePromptVariant(setting, info, type);
    PrioritizeAndMergeNewVariant(current_request_variant, type);
  }

  switch (embedded_prompt_variant_) {
    case Variant::kAsk:
      prompt_view_ = new EmbeddedPermissionPromptAskView(
          browser(), weak_factory_.GetWeakPtr());
      break;
    case Variant::kPreviouslyGranted:
      if (first_prompt) {
        prompt_view_ = new EmbeddedPermissionPromptPreviouslyGrantedView(
            browser(), weak_factory_.GetWeakPtr());
      } else {
        delegate()->FinalizeCurrentRequests();
        return;
      }
      break;
    case Variant::kPreviouslyDenied:
      prompt_view_ = new EmbeddedPermissionPromptPreviouslyDeniedView(
          browser(), weak_factory_.GetWeakPtr());
      break;
    case Variant::kOsPrompt:
      prompt_view_ = new EmbeddedPermissionPromptShowSystemPromptView(
          browser(), weak_factory_.GetWeakPtr());
      break;
    case Variant::kOsSystemSettings:
      prompt_view_ = new EmbeddedPermissionPromptSystemSettingsView(
          browser(), weak_factory_.GetWeakPtr());
      break;
    case Variant::kAdministratorGranted:
      prompt_view_ = new EmbeddedPermissionPromptPolicyView(
          browser(), weak_factory_.GetWeakPtr(),
          /*is_permission_allowed=*/true);
      break;
    case Variant::kAdministratorDenied:
      prompt_view_ = new EmbeddedPermissionPromptPolicyView(
          browser(), weak_factory_.GetWeakPtr(),
          /*is_permission_allowed=*/false);
      break;
    case Variant::kUninitialized:
      NOTREACHED();
  }

  if (prompt_view_) {
    RebuildRequests();
    prompt_view_->Show();
  }
}

void EmbeddedPermissionPrompt::CloseView() {
  if (!prompt_view_) {
    return;
  }

  prompt_view_->PrepareToClose();
  prompt_view_->GetWidget()->Close();
  prompt_view_ = nullptr;

  requests_.clear();
  prompt_types_.clear();
  embedded_prompt_variant_ = Variant::kUninitialized;
}

EmbeddedPermissionPrompt::TabSwitchingBehavior
EmbeddedPermissionPrompt::GetTabSwitchingBehavior() {
  return TabSwitchingBehavior::kKeepPromptAlive;
}

permissions::PermissionPromptDisposition
EmbeddedPermissionPrompt::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE;
}

bool EmbeddedPermissionPrompt::ShouldFinalizeRequestAfterDecided() const {
  return false;
}

void EmbeddedPermissionPrompt::Allow() {
  delegate_->Accept();
  CloseCurrentViewAndMaybeShowNext(/*first_prompt=*/false);
}

void EmbeddedPermissionPrompt::AllowThisTime() {
  delegate_->AcceptThisTime();
  CloseCurrentViewAndMaybeShowNext(/*first_prompt=*/false);
}

void EmbeddedPermissionPrompt::Dismiss() {
  delegate_->Dismiss();
  delegate_->FinalizeCurrentRequests();
}

void EmbeddedPermissionPrompt::Acknowledge() {
  // TOOO(crbug.com/1462930): Find how to distinguish between a dismiss and an
  // acknowledge.
  delegate_->FinalizeCurrentRequests();
}

void EmbeddedPermissionPrompt::StopAllowing() {
  // TODO(crbug.com/1462930): Implement.
  NOTREACHED();
}

void EmbeddedPermissionPrompt::ShowSystemSettings() {
  const auto& requests = delegate()->Requests();
  CHECK_GT(requests.size(), 0U);

// TODO(crbug.com/1462930) Chrome always shows the first permission in a group,
// as it is not possible to open multiple System Setting pages. Figure out a
// better way to handle this scenario.
#if BUILDFLAG(IS_MAC)
  if (requests[0]->request_type() == permissions::RequestType::kCameraStream) {
    OpenCameraSystemSettingsOnMacOS();
  } else if (requests[0]->request_type() ==
             permissions::RequestType::kMicStream) {
    OpenMicSystemSettingsOnMacOS();
  }
#endif
}

void EmbeddedPermissionPrompt::PrioritizeAndMergeNewVariant(
    EmbeddedPermissionPrompt::Variant new_variant,
    ContentSettingsType new_type) {
  // The new variant can be grouped with the already existing one.
  if (CanGroupVariants(embedded_prompt_variant_, new_variant)) {
    prompt_types_.insert(new_type);
    embedded_prompt_variant_ = std::max(embedded_prompt_variant_, new_variant);
  }

  // The existing variant is higher priority than the new one.
  if (embedded_prompt_variant_ > new_variant) {
    return;
  }

  // The new variant has higher priority than the existing one.
  prompt_types_.clear();
  prompt_types_.insert(new_type);
  embedded_prompt_variant_ = new_variant;
}

void EmbeddedPermissionPrompt::RebuildRequests() {
  if (requests_.size() != prompt_types_.size()) {
    auto requests = EmbeddedPermissionPromptBaseView::Delegate::Requests();
    for (auto* request : requests) {
      if (prompt_types_.contains(request->GetContentSettingsType())) {
        requests_.push_back(request);
      }
    }
  }
}
