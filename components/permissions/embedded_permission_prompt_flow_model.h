// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_FLOW_MODEL_H_
#define COMPONENTS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_FLOW_MODEL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_constants.h"
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
  // This class is the platform-independent interface through which the prompt
  // flow model communicates with the scrim UI surface.
  class PromptContentScrim {
   public:
    // Create and display a platform-specific scrim.
    static std::unique_ptr<PromptContentScrim> Create(
        content::WebContents* web_contents,
        EmbeddedPermissionPromptFlowModel* flow_model);

    virtual ~PromptContentScrim() = default;
  };

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

  // Calculate the variant of `request` based on the current state of
  // browser (content settings) and device (settings and policies).
  Variant DeterminePromptVariant(const PermissionRequest* request) const;

  // Calculate the current prompt variant for `requests` and
  // `postponed_requests_`, keeping active matching requests in `requests` and
  // buffering postponed requests in `postponed_requests_`.
  void CalculateCurrentVariant(
      std::vector<std::unique_ptr<PermissionRequest>>& requests);

  content::WebContents* web_contents() const { return web_contents_; }

  Variant prompt_variant() const { return prompt_variant_; }
  void set_prompt_variant(Variant variant) { prompt_variant_ = variant; }

  std::vector<ElementAnchoredBubbleVariant> GetPromptVariants() const;

  void StartFirstDisplayTime() {
    current_variant_first_display_time_ = base::Time::Now();
  }

  void PrecalculateVariantsForMetrics();

  void RecordOsMetrics(permissions::OsScreenAction action);

  void RecordPermissionActionUKM(ElementAnchoredBubbleAction action);

  void RecordElementAnchoredBubbleVariantUMA(Variant variant);

  void DismissScrim();
  PermissionPrompt::Delegate* GetPermissionPromptDelegate() const;

  PromptContentScrim* EnsureContentScrim();
  PromptContentScrim* prompt_content_scrim() { return content_scrim_.get(); }

  std::vector<std::unique_ptr<PermissionRequest>> TakePostponedRequests();

 private:
  Variant prompt_variant_ = Variant::kUninitialized;
  raw_ptr<PermissionPrompt::Delegate> delegate_;

  raw_ptr<content::WebContents> web_contents_;

  int prompt_screen_counter_for_metrics_ = 0;

  // Store precalculated OS variants for metrics
  Variant os_prompt_variant_ = Variant::kUninitialized;
  Variant os_system_settings_variant_ = Variant::kUninitialized;

  base::Time current_variant_first_display_time_;

  // Holds requests that have been postponed by CalculateCurrentVariant().
  std::vector<std::unique_ptr<PermissionRequest>> postponed_requests_;

  // Overall request type for UMA for all requests processed by this flow.
  RequestTypeForUma overall_request_type_for_uma_ = RequestTypeForUma::UNKNOWN;

  std::unique_ptr<PromptContentScrim> content_scrim_;

  base::WeakPtrFactory<EmbeddedPermissionPromptFlowModel> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_FLOW_MODEL_H_
