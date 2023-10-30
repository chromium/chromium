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

void EmbeddedPermissionPrompt::CloseCurrentViewAndMaybeShowNext(
    bool first_prompt) {
  if (!first_prompt) {
    CloseView();
  }

  auto* map = HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  content_settings::SettingInfo info;

  embedded_prompt_variant_ = Variant::kUninitialized;
  for (const auto* request : delegate()->Requests()) {
    ContentSettingsType type = request->GetContentSettingsType();
    ContentSetting setting =
        map->GetContentSetting(delegate()->GetRequestingOrigin(),
                               delegate()->GetEmbeddingOrigin(), type, &info);
    Variant current_request_variant = DeterminePromptVariant(setting, info);
    embedded_prompt_variant_ = HigherPriorityVariant(embedded_prompt_variant_,
                                                     current_request_variant);
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
