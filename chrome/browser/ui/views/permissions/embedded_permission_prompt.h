// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_H_

#include <optional>

#include "base/containers/fixed_flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_view_delegate.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"

class Browser;

namespace content {
class WebContents;
}

class EmbeddedPermissionPrompt
    : public PermissionPromptDesktop,
      public EmbeddedPermissionPromptViewDelegate,
      public EmbeddedPermissionPromptContentScrimView::Delegate {
 public:
  EmbeddedPermissionPrompt(Browser* browser,
                           content::WebContents* web_contents,
                           permissions::PermissionPrompt::Delegate* delegate);
  ~EmbeddedPermissionPrompt() override;
  EmbeddedPermissionPrompt(const EmbeddedPermissionPrompt&) = delete;
  EmbeddedPermissionPrompt& operator=(const EmbeddedPermissionPrompt&) = delete;

  // Prompt views shown after the user clicks on the embedded permission prompt.
  // The values represent the priority of each variant, higher number means
  // higher priority.
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

  // A delegate for handling system permission requests such as requesting new
  // system permission or querying for current system permission settings.
  class SystemPermissionDelegate;

  void CloseCurrentViewAndMaybeShowNext(bool first_prompt);

  // permissions::PermissionPrompt:
  TabSwitchingBehavior GetTabSwitchingBehavior() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;
  bool ShouldFinalizeRequestAfterDecided() const override;
  std::vector<permissions::ElementAnchoredBubbleVariant> GetPromptVariants()
      const override;
  bool IsAskPrompt() const override;
  std::optional<permissions::feature_params::PermissionElementPromptPosition>
  GetPromptPosition() const override;

  // EmbeddedPermissionPromptViewDelegate:
  void Allow() override;
  void AllowThisTime() override;
  void Dismiss() override;
  void Acknowledge() override;
  void StopAllowing() override;
  void ShowSystemSettings() override;
  base::WeakPtr<permissions::PermissionPrompt::Delegate>
  GetPermissionPromptDelegate() const override;
  const std::vector<
      raw_ptr<permissions::PermissionRequest, VectorExperimental>>&
  Requests() const override;

  // EmbeddedPermissionPromptContentScrimView::Delegate:
  void DismissScrim() override;

 private:
  enum class Action {
    kAllow,
    kAllowThisTime,
    kDeny,
    kDismiss,
  };
  Variant DeterminePromptVariant(ContentSetting setting,
                                 const content_settings::SettingInfo& info,
                                 ContentSettingsType type);
  void PrecalculateVariantsForMetrics();
  void PrioritizeAndMergeNewVariant(Variant new_variant,
                                    ContentSettingsType type);

  void RebuildRequests();

  void RecordOsMetrics(permissions::OsScreenAction action);

  void RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction action);

  void PromptForOsPermission();

  void OnRequestSystemPermissionResponse(
      const ContentSettingsType request_type,
      const ContentSettingsType other_request_type);

  void CloseView();

  void FinalizePrompt();
  void SendDelegateAction(Action action);

  // Store precalculated OS variants for metrics
  Variant site_level_prompt_variant_ = Variant::kUninitialized;
  Variant os_prompt_variant_ = Variant::kUninitialized;
  Variant os_system_settings_variant_ = Variant::kUninitialized;

  Variant embedded_prompt_variant_ = Variant::kUninitialized;
  std::unique_ptr<views::Widget> content_scrim_widget_;
  views::ViewTracker prompt_view_tracker_;

  base::Time current_variant_first_display_time_;

  raw_ptr<permissions::PermissionPrompt::Delegate> delegate_;

  std::set<ContentSettingsType> prompt_types_;
  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      requests_;
  int prompt_screen_counter_for_metrics_ = 0;

  std::optional<Action> sent_action_ = std::nullopt;

  base::WeakPtrFactory<EmbeddedPermissionPrompt> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_H_
