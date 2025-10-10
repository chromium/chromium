// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_view_delegate.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/embedded_permission_prompt_flow_model.h"
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
  std::optional<gfx::Rect> GetViewBoundsInScreen() const override;

  // EmbeddedPermissionPromptViewDelegate:
  void Allow() override;
  void AllowThisTime() override;
  void Dismiss() override;
  void Acknowledge() override;
  void StopAllowing() override;
  void ShowSystemSettings() override;
  void SystemPermissionsNoLongerDenied() override;
  base::WeakPtr<permissions::PermissionPrompt::Delegate>
  GetPermissionPromptDelegate() const override;
  const std::vector<base::WeakPtr<permissions::PermissionRequest>>& Requests()
      const override;

  // EmbeddedPermissionPromptContentScrimView::Delegate:
  void DismissScrim() override;

 private:
  enum class Action {
    kAllow,
    kAllowThisTime,
    kDeny,
    kDismiss,
  };

  void PromptForOsPermission();

  void OnRequestSystemPermissionResponse(
      const ContentSettingsType request_type,
      const ContentSettingsType other_request_type);

  void CloseView();
  void CloseViewAndScrim();

  void FinalizePrompt();
  void SendDelegateAction(Action action);

  permissions::EmbeddedPermissionPromptFlowModel::Variant prompt_variant()
      const {
    return prompt_model_->prompt_variant();
  }

  std::unique_ptr<views::Widget> content_scrim_widget_;
  views::ViewTracker prompt_view_tracker_;
  std::unique_ptr<tabs::ScopedTabModalUI> scoped_tab_modal_ui_;
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;

  raw_ptr<permissions::PermissionPrompt::Delegate> delegate_;

  std::set<ContentSettingsType> prompt_types_;
  std::vector<base::WeakPtr<permissions::PermissionRequest>> requests_;

  std::unique_ptr<permissions::EmbeddedPermissionPromptFlowModel> prompt_model_;
  base::WeakPtrFactory<EmbeddedPermissionPrompt> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_H_
