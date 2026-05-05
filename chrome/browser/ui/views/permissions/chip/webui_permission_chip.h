// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_WEBUI_PERMISSION_CHIP_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_WEBUI_PERMISSION_CHIP_H_

#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_interface.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_theme.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/permissions/permission_actions_history.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class WebUILocationBar;

class WebUIPermissionChip : public PermissionChipInterface {
 public:
  explicit WebUIPermissionChip(WebUILocationBar* location_bar);
  ~WebUIPermissionChip() override;

  // PermissionChipInterface:
  void SetVisible(bool visible) override;
  bool GetVisible() const override;
  void SetChipIcon(const gfx::VectorIcon& icon) override;
  void SetChipIcon(const gfx::VectorIcon* icon) override;
  void SetMessage(std::u16string message) override;
  void SetTooltipText(const std::u16string& tooltip) override;
  void SetTheme(PermissionChipTheme theme) override;
  void SetUserDecision(permissions::PermissionAction user_decision) override;
  void SetBlockedIconShowing(bool should_show_blocked_icon) override;
  void SetPermissionPromptStyle(PermissionPromptStyle prompt_style) override;
  void AnimateCollapse(base::TimeDelta duration) override;
  void AnimateExpand(base::TimeDelta duration) override;
  void AnimateToFit(base::TimeDelta duration) override;
  void ResetAnimation(AnimationState state) override;
  bool IsFullyCollapsed() const override;
  bool IsAnimating() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  [[nodiscard]] base::CallbackListSubscription AddVisibilityCallback(
      base::RepeatingClosure callback) override;
  void SetAccessibilityIgnored(bool is_ignored) override;
  void SetAccessibilityName(const std::u16string& name) override;
  void AnnounceText(const std::u16string& text) override;
  void AnnounceAlert(const std::u16string& text) override;
  bool IsMouseHovered() const override;
  void SetPressedCallback(base::RepeatingClosure callback) override;
  views::BubbleAnchor GetAnchor() override;
  void SetBubbleOwner(BubbleOwnerDelegate* owner) override;

  // Called from WebUI
  void OnExpandAnimationEnded();
  void OnCollapseAnimationEnded();
  void OnMousePressed();
  void OnClicked();
  void OnMouseEntered();
  void OnMouseExited();

  // Returns the declarative target state for the WebUI frontend.
  // Note: For animations, this returns what the UI *should* transition to
  // (e.g., `should_collapse_`), not the instantaneous physical state of the UI.
  toolbar_ui_api::mojom::PermissionChipStatePtr GetState() const;

 private:
  void NotifyVisibilityChanged();
  void UpdateState();

  raw_ptr<WebUILocationBar> location_bar_;

  bool is_visible_ = false;
  std::string icon_name_;
  std::u16string message_;
  std::u16string tooltip_;
  PermissionChipTheme theme_ = PermissionChipTheme::kNormalVisibility;
  permissions::PermissionAction user_decision_ =
      permissions::PermissionAction::GRANTED;
  bool should_show_blocked_icon_ = false;
  PermissionPromptStyle prompt_style_ = PermissionPromptStyle::kChip;

  // True only when the chip has fully finished its collapse animation.
  bool is_fully_collapsed_ = false;
  // Collapse request sent over Mojo to the WebUI, which instantly triggers CSS
  // animations on the frontend.
  bool should_collapse_ = false;

  bool is_animating_ = false;
  std::u16string accessibility_name_;
  bool is_mouse_hovered_ = false;

  raw_ptr<BubbleOwnerDelegate> bubble_owner_ = nullptr;

  base::RepeatingClosure pressed_callback_;
  base::ObserverList<Observer> observers_;
  base::RepeatingClosureList visibility_callbacks_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_WEBUI_PERMISSION_CHIP_H_
