// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/embedded_permission_prompt_flow_model.h"

#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/web_contents.h"
#if BUILDFLAG(IS_ANDROID)
#include "components/permissions/android/android_permission_util.h"
#endif

namespace {

using content_settings::SettingSource;

using Variant = permissions::EmbeddedPermissionPromptFlowModel::Variant;

// An upper bound on the maximum number of screens that we can record in
// metrics. Practically speaking the actual number should never be more than 3
// but a higher bound allows us to detect via metrics if this happens in the
// wild.
constexpr int kScreenCounterMaximum = 10;

bool CanGroupVariants(Variant a, Variant b) {
  // Ask and PreviouslyDenied are a special case and can be grouped together.
  if ((a == Variant::kPreviouslyDenied && b == Variant::kAsk) ||
      (a == Variant::kAsk && b == Variant::kPreviouslyDenied)) {
    return true;
  }

  return (a == b);
}

permissions::ElementAnchoredBubbleVariant GetElementAnchoredBubbleVariant(
    Variant variant) {
  switch (variant) {
    case Variant::kUninitialized:
      return permissions::ElementAnchoredBubbleVariant::kUninitialized;
    case Variant::kAdministratorGranted:
      return permissions::ElementAnchoredBubbleVariant::kAdministratorGranted;
    case Variant::kPreviouslyGranted:
      return permissions::ElementAnchoredBubbleVariant::kPreviouslyGranted;
    case Variant::kOsSystemSettings:
      return permissions::ElementAnchoredBubbleVariant::kOsSystemSettings;
    case Variant::kOsPrompt:
      return permissions::ElementAnchoredBubbleVariant::kOsPrompt;
    case Variant::kAsk:
      return permissions::ElementAnchoredBubbleVariant::kAsk;
    case Variant::kPreviouslyDenied:
      return permissions::ElementAnchoredBubbleVariant::kPreviouslyDenied;
    case Variant::kAdministratorDenied:
      return permissions::ElementAnchoredBubbleVariant::kAdministratorDenied;
  }

  NOTREACHED();
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
    PermissionSetting setting,
    const content_settings::SettingInfo& info,
    ContentSettingsType type) {
  // If the administrator blocked the permission, there is nothing the user can
  // do. Presenting them with a different screen in unproductive.
  if (PermissionsClient::Get()->IsPermissionBlockedByDevicePolicy(
          web_contents(), setting, info, type)) {
    return Variant::kAdministratorDenied;
  }

  auto* permission_info =
      content_settings::PermissionSettingsRegistry::GetInstance()->Get(type);

#if BUILDFLAG(IS_ANDROID)
  if (!HasSystemPermission(type, web_contents_) &&
      !CanRequestSystemPermission(type, web_contents_)) {
    return Variant::kOsSystemSettings;
  }
  if (permission_info->delegate().IsAnyPermissionAllowed(setting) &&
      !HasSystemPermission(type, web_contents_) &&
      CanRequestSystemPermission(type, web_contents_)) {
    return Variant::kOsPrompt;
  }
#else
  // Determine if we can directly show one of the OS views. The "System
  // Settings" view is higher priority then all the other remaining options,
  // whereas the "OS Prompt" view is only higher priority then the views that
  // are associated with a site-level allowed state.
  // TODO(crbug.com/40275129): Handle going to Windows settings.
  if (PermissionsClient::Get()->IsSystemDenied(type)) {
    return Variant::kOsSystemSettings;
  }

  if (permission_info->delegate().IsAnyPermissionAllowed(setting) &&
      PermissionsClient::Get()->CanPromptSystemPermission(type)) {
    return Variant::kOsPrompt;
  }
#endif

  if (PermissionsClient::Get()->IsPermissionAllowedByDevicePolicy(
          web_contents(), setting, info, type)) {
    return Variant::kAdministratorGranted;
  }

  if (permission_info->delegate().IsUndecided(setting)) {
    return Variant::kAsk;
  } else if (permission_info->delegate().IsAnyPermissionAllowed(setting)) {
    return Variant::kPreviouslyGranted;
  } else {
    DCHECK(permission_info->delegate().IsBlocked(setting));
    return Variant::kPreviouslyDenied;
  }
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
  Clear();
  auto* map = PermissionsClient::Get()->GetSettingsMap(
      web_contents()->GetBrowserContext());
  content_settings::SettingInfo info;

  for (const auto& request : delegate_->Requests()) {
    ContentSettingsType type = request->GetContentSettingsType();
    PermissionSetting setting =
        map->GetPermissionSetting(delegate_->GetRequestingOrigin(),
                                  delegate_->GetEmbeddingOrigin(), type, &info);
    Variant current_request_variant =
        DeterminePromptVariant(setting, info, type);
    PrioritizeAndMergeNewVariant(current_request_variant, type);
  }

  const auto& requests = delegate_->Requests();
  for (const auto& request : requests) {
    if (prompt_types_.contains(request->GetContentSettingsType())) {
      requests_.push_back(request->GetWeakPtr());
    }
  }
}

void EmbeddedPermissionPromptFlowModel::PrecalculateVariantsForMetrics() {
  if (prompt_variant() == Variant::kUninitialized) {
    return;
  }

  if (os_prompt_variant_ == Variant::kUninitialized) {
    for (const auto& request : delegate_->Requests()) {
      const auto& type = request->GetContentSettingsType();
#if BUILDFLAG(IS_ANDROID)
      if (!HasSystemPermission(type, web_contents_) &&
          CanRequestSystemPermission(type, web_contents_)) {
#else
      if (PermissionsClient::Get()->CanPromptSystemPermission(type)) {
#endif
        os_prompt_variant_ = Variant::kOsPrompt;
        break;
      }
    }
  }

  if (os_system_settings_variant_ == Variant::kUninitialized) {
    for (const auto& request : delegate_->Requests()) {
      const auto& type = request->GetContentSettingsType();
#if BUILDFLAG(IS_ANDROID)
      if (!HasSystemPermission(type, web_contents_) &&
          !CanRequestSystemPermission(type, web_contents_)) {
#else
      if (PermissionsClient::Get()->IsSystemDenied(type)) {
#endif
        os_system_settings_variant_ = Variant::kOsSystemSettings;
        break;
      }
    }
  }
}

void EmbeddedPermissionPromptFlowModel::RecordOsMetrics(
    permissions::OsScreenAction action) {
  const auto& requests = delegate_->Requests();
  CHECK_GT(requests.size(), 0U);

  permissions::OsScreen screen;

  switch (prompt_variant()) {
    case Variant::kOsPrompt:
      screen = permissions::OsScreen::kOsPrompt;
      break;
    case Variant::kOsSystemSettings:
      screen = permissions::OsScreen::kOsSystemSettings;
      break;
    default:
      return;
  }

  base::TimeDelta time_to_decision =
      base::Time::Now() - current_variant_first_display_time_;
  permissions::PermissionUmaUtil::RecordElementAnchoredBubbleOsMetrics(
      requests, screen, action, time_to_decision);
}

void EmbeddedPermissionPromptFlowModel::RecordPermissionActionUKM(
    permissions::ElementAnchoredBubbleAction action) {
  // There should never be more than kScreenCounterMaximum screens. If this is
  // hit something has gone wrong and we're probably caught in a loop showing
  // the same screens over and over.
  DCHECK_LE(prompt_screen_counter_for_metrics_, kScreenCounterMaximum);

  permissions::PermissionUmaUtil::RecordElementAnchoredPermissionPromptAction(
      // This represents all the requests for the entire prompt.
      delegate_->Requests(),
      // This only contains the requests for the currently active screen, which
      // could sometimes be a subset of all requests for the entire prompt.
      requests(), action, GetElementAnchoredBubbleVariant(prompt_variant()),
      prompt_screen_counter_for_metrics_, delegate_->GetRequestingOrigin(),
      delegate_->GetAssociatedWebContents(),
      delegate_->GetAssociatedWebContents()->GetBrowserContext());

  ++prompt_screen_counter_for_metrics_;
}

void EmbeddedPermissionPromptFlowModel::RecordElementAnchoredBubbleVariantUMA(
    Variant variant) {
  permissions::PermissionUmaUtil::RecordElementAnchoredBubbleVariantUMA(
      delegate_->Requests(), GetElementAnchoredBubbleVariant(variant));
}

std::vector<permissions::ElementAnchoredBubbleVariant>
EmbeddedPermissionPromptFlowModel::GetPromptVariants() const {
  std::vector<permissions::ElementAnchoredBubbleVariant> variants;

  // Current prompt variant when the user takes an action on a site level
  // prompt.
  if (prompt_variant() != Variant::kUninitialized) {
    variants.push_back(GetElementAnchoredBubbleVariant(prompt_variant()));
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  if (os_prompt_variant_ != Variant::kUninitialized) {
    variants.push_back(GetElementAnchoredBubbleVariant(os_prompt_variant_));
  }
  if (os_system_settings_variant_ != Variant::kUninitialized) {
    variants.push_back(
        GetElementAnchoredBubbleVariant(os_system_settings_variant_));
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

  return variants;
}

void EmbeddedPermissionPromptFlowModel::SetDelegateAction(
    DelegateAction action) {
  if (action_.has_value()) {
    return;
  }

  action_ = action;
  switch (action) {
    case DelegateAction::kAllow:
      delegate_->Accept();
      break;
    case DelegateAction::kAllowThisTime:
      delegate_->AcceptThisTime();
      break;
    case DelegateAction::kDeny:
      delegate_->Deny();
      break;
    case DelegateAction::kDismiss:
      delegate_->Dismiss();
      break;
  }
}

}  // namespace permissions
