// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/embedded_permission_prompt_flow_model.h"

#include <variant>

#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_uma_constants.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
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

EmbeddedPermissionPromptFlowModel::PromptContentScrim*
EmbeddedPermissionPromptFlowModel::EnsureContentScrim() {
  if (!content_scrim_) {
    content_scrim_ = PromptContentScrim::Create(web_contents_, this);
  }
  return content_scrim_.get();
}

// static
std::unique_ptr<EmbeddedPermissionPromptFlowModel::PromptContentScrim>
EmbeddedPermissionPromptFlowModel::PromptContentScrim::Create(
    content::WebContents* web_contents,
    EmbeddedPermissionPromptFlowModel* flow_model) {
  return PermissionsClient::Get()->CreatePromptContentScrim(web_contents,
                                                            flow_model);
}

EmbeddedPermissionPromptFlowModel::Variant
EmbeddedPermissionPromptFlowModel::DeterminePromptVariant(
    const PermissionRequest* request) const {
  ContentSettingsType type = request->GetContentSettingsType();
  auto* map = PermissionsClient::Get()->GetSettingsMap(
      web_contents()->GetBrowserContext());
  content_settings::SettingInfo info;
  PermissionSetting setting = map->GetPermissionSetting(
      request->requesting_origin(), request->embedding_origin(), type, &info);

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

void EmbeddedPermissionPromptFlowModel::CalculateCurrentVariant(
    std::vector<std::unique_ptr<PermissionRequest>>& requests) {
  if (overall_request_type_for_uma_ == RequestTypeForUma::UNKNOWN) {
    overall_request_type_for_uma_ =
        PermissionUtil::GetUmaValueForRequests(requests);
  }

  for (auto& request : postponed_requests_) {
    requests.push_back(std::move(request));
  }
  postponed_requests_.clear();

  if (requests.empty()) {
    prompt_variant_ = Variant::kUninitialized;
    return;
  }

  Variant highest_priority_variant = Variant::kUninitialized;

  for (const auto& request : requests) {
    Variant request_variant = DeterminePromptVariant(request.get());
    if (CanGroupVariants(highest_priority_variant, request_variant)) {
      highest_priority_variant =
          std::max(highest_priority_variant, request_variant);
    } else if (request_variant > highest_priority_variant) {
      highest_priority_variant = request_variant;
    }
  }

  std::vector<std::unique_ptr<permissions::PermissionRequest>> active_requests;

  for (auto& request : requests) {
    Variant request_variant = DeterminePromptVariant(request.get());
    if (CanGroupVariants(highest_priority_variant, request_variant)) {
      active_requests.push_back(std::move(request));
    } else {
      postponed_requests_.push_back(std::move(request));
    }
  }

  requests = std::move(active_requests);
  prompt_variant_ = highest_priority_variant;
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
      *delegate_->Requests()[0], overall_request_type_for_uma_,
      PermissionUtil::GetUmaValueForRequests(delegate_->Requests()), action,
      GetElementAnchoredBubbleVariant(prompt_variant()),
      prompt_screen_counter_for_metrics_, delegate_->GetRequestingOrigin(),
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

void EmbeddedPermissionPromptFlowModel::DismissScrim() {
  PermissionUmaUtil::RecordElementAnchoredBubbleDismiss(
      delegate_->Requests(), DismissedReason::kDismissedScrim);
  RecordOsMetrics(OsScreenAction::kDismissedScrim);
  RecordPermissionActionUKM(ElementAnchoredBubbleAction::kDismissedScrim);

  PrecalculateVariantsForMetrics();

  CHECK_NE(delegate_->Requests()[0]->GetContentSettingsType(),
           ContentSettingsType::GEOLOCATION_WITH_OPTIONS);

  delegate_->Dismiss(/*prompt_options=*/std::monostate());
}

PermissionPrompt::Delegate*
EmbeddedPermissionPromptFlowModel::GetPermissionPromptDelegate() const {
  return delegate_;
}

std::vector<std::unique_ptr<PermissionRequest>>
EmbeddedPermissionPromptFlowModel::TakePostponedRequests() {
  std::vector<std::unique_ptr<PermissionRequest>> postponed_requests =
      std::move(postponed_requests_);
  postponed_requests_.clear();
  return postponed_requests;
}

}  // namespace permissions
