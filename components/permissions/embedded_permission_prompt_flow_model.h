// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_FLOW_MODEL_H_
#define COMPONENTS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_FLOW_MODEL_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"

namespace content {
class WebContents;
}  // namespace content

namespace permissions {

// This class figures out how to calculate the right prompt variant, looking at
// the ongoing permission requests, content settings, and system
// settings/policies. The later `*_Permission_Prompt` class will use this shared
// logic to find the correct screen to show the user.
class EmbeddedPermissionPromptFlowModel {
 public:
  EmbeddedPermissionPromptFlowModel(content::WebContents* web_contents,
                                    PermissionPrompt::Delegate* delegate);
  ~EmbeddedPermissionPromptFlowModel();
  EmbeddedPermissionPromptFlowModel(const EmbeddedPermissionPromptFlowModel&) =
      delete;
  EmbeddedPermissionPromptFlowModel& operator=(
      const EmbeddedPermissionPromptFlowModel&) = delete;

  // Prompt views shown after the user clicks on the embedded permission prompt.
  // The values represent the priority of each variant, higher number means
  // higher priority.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.permissions
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: EmbeddedPromptVariant
  enum class Variant {
    // Default when conditions are not met to show any of the permission views.
    kUninitialized = 0,
    // Informs the user that the permission was allowed by their administrator.
    kAdministratorGranted = 1,
    // Permission prompt that informs the user they already granted permission.
    // Offers additional options to modify the permission decision.
    kPreviouslyGranted = 2,
    // Informs the user that Chrome needs permission from the OS level, in order
    // for the site to be able to access a permission.
    kOsPrompt = 3,
    // Permission prompt that asks the user for site-level permission.
    kAsk = 4,
    // Permission prompt that additionally informs the user that they have
    // previously denied permission to the site. May offer different options
    // (buttons) to the site-level prompt |kAsk|.
    kPreviouslyDenied = 5,
    // Informs the user that they need to go to OS system settings to grant
    // access to Chrome.
    kOsSystemSettings = 6,
    // Informs the user that the permission was denied by their administrator.
    kAdministratorDenied = 7,
  };

  // Define the unique delegate action owned by this model.
  enum class DelegateAction {
    kAllow,
    kAllowThisTime,
    kDeny,
    kDismiss,
  };

  // Calculate the variant of given type based on the current state of browser
  // (content settings) and device (settings and policies).
  Variant DeterminePromptVariant(PermissionSetting setting,
                                 const content_settings::SettingInfo& info,
                                 ContentSettingsType type);

  // Compare current variant with the new one based on the prioty order,
  // grouping if necessary.
  void PrioritizeAndMergeNewVariant(Variant new_variant,
                                    ContentSettingsType type);

  content::WebContents* web_contents() const { return web_contents_; }

  // Calculate the current prompt variant for the ongoing permission requests.
  void CalculateCurrentVariant();

  Variant prompt_variant() const { return prompt_variant_; }

  std::vector<permissions::ElementAnchoredBubbleVariant> GetPromptVariants()
      const;

  const std::set<ContentSettingsType>& prompt_types() const {
    return prompt_types_;
  }

  const std::vector<base::WeakPtr<permissions::PermissionRequest>>& requests()
      const {
    return requests_;
  }

  void Clear() {
    requests_.clear();
    prompt_variant_ = Variant::kUninitialized;
    prompt_types_.clear();
  }

  void StartFirstDisplayTime() {
    current_variant_first_display_time_ = base::Time::Now();
  }

  void PrecalculateVariantsForMetrics();

  void RecordOsMetrics(permissions::OsScreenAction action);

  void RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction action);

  void RecordElementAnchoredBubbleVariantUMA(Variant variant);

  void SetDelegateAction(DelegateAction action);

  bool HasDelegateActionSet() const { return action_.has_value(); }

 private:
  Variant prompt_variant_ = Variant::kUninitialized;
  raw_ptr<PermissionPrompt::Delegate> delegate_;

  std::set<ContentSettingsType> prompt_types_;
  std::vector<base::WeakPtr<permissions::PermissionRequest>> requests_;

  raw_ptr<content::WebContents> web_contents_;

  int prompt_screen_counter_for_metrics_ = 0;

  // Store precalculated OS variants for metrics
  Variant os_prompt_variant_ = Variant::kUninitialized;
  Variant os_system_settings_variant_ = Variant::kUninitialized;

  base::Time current_variant_first_display_time_;

  std::optional<DelegateAction> action_ = std::nullopt;

  base::WeakPtrFactory<EmbeddedPermissionPromptFlowModel> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_FLOW_MODEL_H_
