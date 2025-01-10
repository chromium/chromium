// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/embedded_permission_prompt_flow_model.h"

#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/web_contents.h"

namespace {

using content_settings::SettingSource;

bool CanGroupVariants(
    permissions::EmbeddedPermissionPromptFlowModel::Variant a,
    permissions::EmbeddedPermissionPromptFlowModel::Variant b) {
  // Ask and PreviouslyDenied are a special case and can be grouped together.
  if ((a == permissions::EmbeddedPermissionPromptFlowModel::Variant::
                kPreviouslyDenied &&
       b == permissions::EmbeddedPermissionPromptFlowModel::Variant::kAsk) ||
      (a == permissions::EmbeddedPermissionPromptFlowModel::Variant::kAsk &&
       b == permissions::EmbeddedPermissionPromptFlowModel::Variant::
                kPreviouslyDenied)) {
    return true;
  }

  return (a == b);
}

}  // namespace

namespace permissions {

EmbeddedPermissionPromptFlowModel::EmbeddedPermissionPromptFlowModel(
    content::WebContents* web_contents,
    PermissionPrompt::Delegate* delegate)
    : delegate_(delegate), web_contents_(web_contents) {}
EmbeddedPermissionPromptFlowModel::~EmbeddedPermissionPromptFlowModel() =
    default;

EmbeddedPermissionPromptFlowModel::Variant
EmbeddedPermissionPromptFlowModel::DeterminePromptVariant(
    ContentSetting setting,
    const content_settings::SettingInfo& info,
    ContentSettingsType type) {
  // If the administrator blocked the permission, there is nothing the user can
  // do. Presenting them with a different screen in unproductive.
  if (PermissionsClient::Get()->IsPermissionBlockedByDevicePolicy(
          web_contents(), setting, info, type)) {
    return Variant::kAdministratorDenied;
  }

  // Determine if we can directly show one of the OS views. The "System
  // Settings" view is higher priority then all the other remaining options,
  // whereas the "OS Prompt" view is only higher priority then the views that
  // are associated with a site-level allowed state.
  // TODO(crbug.com/40275129): Handle going to Windows settings.
  if (PermissionsClient::Get()->IsSystemDenied(type)) {
    return Variant::kOsSystemSettings;
  }

  if (setting == CONTENT_SETTING_ALLOW &&
      PermissionsClient::Get()->CanPromptSystemPermission(type)) {
    return Variant::kOsPrompt;
  }

  if (PermissionsClient::Get()->IsPermissionAllowedByDevicePolicy(
          web_contents(), setting, info, type)) {
    return Variant::kAdministratorGranted;
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

void EmbeddedPermissionPromptFlowModel::PrioritizeAndMergeNewVariant(
    EmbeddedPermissionPromptFlowModel::Variant new_variant,
    ContentSettingsType new_type) {
  // The new variant can be grouped with the already existing one.
  if (CanGroupVariants(prompt_variant_, new_variant)) {
    prompt_types_.insert(new_type);
    prompt_variant_ = std::max(prompt_variant_, new_variant);
    return;
  }

  // The existing variant is higher priority than the new one.
  if (prompt_variant_ > new_variant) {
    return;
  }

  // The new variant has higher priority than the existing one.
  prompt_types_.clear();
  prompt_types_.insert(new_type);
  prompt_variant_ = new_variant;
}

void EmbeddedPermissionPromptFlowModel::CalculateCurrentVariant() {
  auto* map = PermissionsClient::Get()->GetSettingsMap(
      web_contents()->GetBrowserContext());
  content_settings::SettingInfo info;

  for (const auto& request : delegate_->Requests()) {
    ContentSettingsType type = request->GetContentSettingsType();
    ContentSetting setting =
        map->GetContentSetting(delegate_->GetRequestingOrigin(),
                               delegate_->GetEmbeddingOrigin(), type, &info);
    Variant current_request_variant =
        DeterminePromptVariant(setting, info, type);
    PrioritizeAndMergeNewVariant(current_request_variant, type);
  }

  if (requests_.size() != prompt_types_.size()) {
    const auto& requests = delegate_->Requests();
    for (PermissionRequest* request : requests) {
      if (prompt_types_.contains(request->GetContentSettingsType())) {
        requests_.push_back(request);
      }
    }
  }
}

}  // namespace permissions
