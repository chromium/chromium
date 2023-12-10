// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt.h"

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_ask_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"
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

  raw_ptr<EmbeddedPermissionPromptBaseView> prompt_view = nullptr;

  switch (embedded_prompt_variant_) {
    case Variant::kAsk:
      prompt_view = new EmbeddedPermissionPromptAskView(
          browser(), weak_factory_.GetWeakPtr());
      break;
    case Variant::kPreviouslyGranted:
      if (first_prompt) {
        prompt_view = new EmbeddedPermissionPromptPreviouslyGrantedView(
            browser(), weak_factory_.GetWeakPtr());
      } else {
        delegate()->FinalizeCurrentRequests();
        return;
      }
      break;
    case Variant::kPreviouslyDenied:
      prompt_view = new EmbeddedPermissionPromptPreviouslyDeniedView(
          browser(), weak_factory_.GetWeakPtr());
      break;
    case Variant::kOsPrompt:
      prompt_view = new EmbeddedPermissionPromptShowSystemPromptView(
          browser(), weak_factory_.GetWeakPtr());
// This view has no buttons, so the OS level prompt should be triggered at the
// same time as the |EmbeddedPermissionPromptShowSystemPromptView|.
#if BUILDFLAG(IS_MAC)
      PromptForOsPermission();
#endif
      break;
    case Variant::kOsSystemSettings:
      prompt_view = new EmbeddedPermissionPromptSystemSettingsView(
          browser(), weak_factory_.GetWeakPtr());
      break;
    case Variant::kAdministratorGranted:
      prompt_view = new EmbeddedPermissionPromptPolicyView(
          browser(), weak_factory_.GetWeakPtr(),
          /*is_permission_allowed=*/true);
      break;
    case Variant::kAdministratorDenied:
      prompt_view = new EmbeddedPermissionPromptPolicyView(
          browser(), weak_factory_.GetWeakPtr(),
          /*is_permission_allowed=*/false);
      break;
    case Variant::kUninitialized:
      NOTREACHED();
  }

  if (prompt_view) {
    RebuildRequests();
    prompt_view_tracker_.SetView(prompt_view);
    content_scrim_widget_ =
        EmbeddedPermissionPromptContentScrimView::CreateScrimWidget(
            weak_factory_.GetWeakPtr());
    prompt_view->UpdateAnchor(content_scrim_widget_.get());
    prompt_view->Show();
  }
}

EmbeddedPermissionPrompt::TabSwitchingBehavior
EmbeddedPermissionPrompt::GetTabSwitchingBehavior() {
  return TabSwitchingBehavior::kDestroyPromptButKeepRequestPending;
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
  CloseView();
  delegate_->FinalizeCurrentRequests();
}

void EmbeddedPermissionPrompt::StopAllowing() {
  delegate_->Deny();
  delegate_->FinalizeCurrentRequests();
}

void EmbeddedPermissionPrompt::ShowSystemSettings() {
  const auto& requests = delegate()->Requests();
  CHECK_GT(requests.size(), 0U);
// TODO(crbug.com/1462930) Chrome always shows the first permission in a group,
// as it is not possible to open multiple System Setting pages. Figure out a
// better way to handle this scenario.
#if BUILDFLAG(IS_MAC)
  if (requests_[0]->request_type() == permissions::RequestType::kCameraStream) {
    OpenCameraSystemSettingsOnMacOS();
  } else if (requests_[0]->request_type() ==
             permissions::RequestType::kMicStream) {
    OpenMicSystemSettingsOnMacOS();
  }
#endif
}

void EmbeddedPermissionPrompt::DismissScrim() {
  CloseView();
  Dismiss();
}

base::WeakPtr<permissions::PermissionPrompt::Delegate>
EmbeddedPermissionPrompt::GetPermissionPromptDelegate() const {
  return delegate_->GetWeakPtr();
}

const std::vector<permissions::PermissionRequest*>&
EmbeddedPermissionPrompt::Requests() const {
  return requests_;
}

void EmbeddedPermissionPrompt::PromptForOsPermission() {
#if BUILDFLAG(IS_MAC)
  // We currently support <=2 grouped permissions.
  CHECK_LE(prompt_types_.size(), 2U);

  for (const auto prompt : prompt_types_) {
    RequestMacOSMediaSystemPermission(prompt, prompt_types_.size() == 2U);
  }
#endif
}

#if BUILDFLAG(IS_MAC)
void EmbeddedPermissionPrompt::OnRequestSystemMediaPermissionResponse(
    const ContentSettingsType request_type,
    bool grouped_permissions) {
  system_media_permissions::SystemPermission permission,
      other_permission =
          system_media_permissions::SystemPermission::kNotDetermined;

  if (request_type == ContentSettingsType::MEDIASTREAM_MIC) {
    permission = system_media_permissions::CheckSystemAudioCapturePermission();
    other_permission =
        grouped_permissions
            ? system_media_permissions::CheckSystemVideoCapturePermission()
            : system_media_permissions::SystemPermission::kNotDetermined;
  }

  if (request_type == ContentSettingsType::MEDIASTREAM_CAMERA) {
    permission = system_media_permissions::CheckSystemVideoCapturePermission();
    other_permission =
        grouped_permissions
            ? system_media_permissions::CheckSystemAudioCapturePermission()
            : system_media_permissions::SystemPermission::kNotDetermined;
  }

  switch (permission) {
    case system_media_permissions::SystemPermission::kRestricted:
    case system_media_permissions::SystemPermission::kDenied:
    case system_media_permissions::SystemPermission::kAllowed:
      // Do not finalize request until all the necessary system permissions are
      // granted.
      if (!grouped_permissions ||
          other_permission !=
              system_media_permissions::SystemPermission::kNotDetermined) {
        CloseView();
        delegate_->FinalizeCurrentRequests();
      }
      break;
    default:
      NOTREACHED();
  }
}

// TODO: Refactor this logic for PEPC and other permission prompts, to avoid
// code duplication.
void EmbeddedPermissionPrompt::RequestMacOSMediaSystemPermission(
    const ContentSettingsType request_type,
    bool grouped_permissions) {
  if (request_type == ContentSettingsType::MEDIASTREAM_MIC) {
    system_media_permissions::RequestSystemAudioCapturePermission(
        base::BindOnce(
            &EmbeddedPermissionPrompt::OnRequestSystemMediaPermissionResponse,
            weak_factory_.GetWeakPtr(), request_type, grouped_permissions));
    return;
  }

  if (request_type == ContentSettingsType::MEDIASTREAM_CAMERA) {
    system_media_permissions::RequestSystemVideoCapturePermission(
        base::BindOnce(
            &EmbeddedPermissionPrompt::OnRequestSystemMediaPermissionResponse,
            weak_factory_.GetWeakPtr(), request_type, grouped_permissions));
    return;
  }
}
#endif

void EmbeddedPermissionPrompt::PrioritizeAndMergeNewVariant(
    EmbeddedPermissionPrompt::Variant new_variant,
    ContentSettingsType new_type) {
  // The new variant can be grouped with the already existing one.
  if (CanGroupVariants(embedded_prompt_variant_, new_variant)) {
    prompt_types_.insert(new_type);
    embedded_prompt_variant_ = std::max(embedded_prompt_variant_, new_variant);
    return;
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
    const auto& requests = delegate()->Requests();
    for (auto* request : requests) {
      if (prompt_types_.contains(request->GetContentSettingsType())) {
        requests_.push_back(request);
      }
    }
  }
}

void EmbeddedPermissionPrompt::CloseView() {
  if (auto* prompt_view = static_cast<EmbeddedPermissionPromptBaseView*>(
          prompt_view_tracker_.view())) {
    prompt_view->PrepareToClose();
    prompt_view->GetWidget()->Close();
    prompt_view_tracker_.SetView(nullptr);

    requests_.clear();
    prompt_types_.clear();
    embedded_prompt_variant_ = Variant::kUninitialized;
  }

  if (content_scrim_widget_) {
    content_scrim_widget_->Close();
    content_scrim_widget_.reset();
  }
}
