// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_CHIP_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_CHIP_INTERFACE_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_theme.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "components/permissions/permission_actions_history.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
struct VectorIcon;
}

// An abstract interface for manipulating a permission chip (such as an
// activity indicator or a permission request).
class PermissionChipInterface {
 public:
  // Delegate providing state information about the bubble anchored to this
  // chip. This allows internal chip components (like hover controllers) to know
  // if they should pause or restart collapse timers based on the bubble's
  // visibility and animation state.
  class BubbleOwnerDelegate {
   public:
    virtual ~BubbleOwnerDelegate() = default;
    virtual bool IsBubbleShowing() = 0;
    virtual bool IsAnimating() const = 0;
    virtual void RestartTimersOnMouseHover() = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnChipVisibilityChanged(bool is_visible) {}
    virtual void OnExpandAnimationEnded() {}
    virtual void OnCollapseAnimationEnded() {}
    virtual void OnMousePressed() {}
  };

  virtual ~PermissionChipInterface() = default;

  virtual void SetVisible(bool visible) = 0;
  virtual bool GetVisible() const = 0;

  // Customize the chip.
  virtual void SetChipIcon(const gfx::VectorIcon& icon) = 0;
  virtual void SetChipIcon(const gfx::VectorIcon* icon) = 0;
  virtual void SetMessage(std::u16string message) = 0;
  virtual void SetTooltipText(const std::u16string& tooltip) = 0;
  virtual void SetTheme(PermissionChipTheme theme) = 0;
  virtual void SetUserDecision(permissions::PermissionAction user_decision) = 0;
  virtual void SetBlockedIconShowing(bool should_show_blocked_icon) = 0;
  virtual void SetPermissionPromptStyle(PermissionPromptStyle prompt_style) = 0;

  // Animation methods.
  virtual void AnimateCollapse(base::TimeDelta duration) = 0;
  virtual void AnimateExpand(base::TimeDelta duration) = 0;
  virtual void AnimateToFit(base::TimeDelta duration) = 0;

  enum class AnimationState {
    kCollapsed,
    kExpanded,
  };
  virtual void ResetAnimation(AnimationState state) = 0;
  virtual bool IsFullyCollapsed() const = 0;
  virtual bool IsAnimating() const = 0;

  // Add/remove observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Add a callback to be called when the chip's visibility changes.
  [[nodiscard]] virtual base::CallbackListSubscription AddVisibilityCallback(
      base::RepeatingClosure callback) = 0;

  virtual void SetAccessibilityIgnored(bool is_ignored) = 0;
  virtual void SetAccessibilityName(const std::u16string& name) = 0;
  virtual void AnnounceText(const std::u16string& text) = 0;
  virtual void AnnounceAlert(const std::u16string& text) = 0;

  // Note: For WebUI implementations, this value will be approximate as it
  // relies on asynchronous IPCs from the renderer process.
  virtual bool IsMouseHovered() const = 0;

  // Set the callback invoked when the chip is pressed.
  virtual void SetPressedCallback(base::RepeatingClosure callback) = 0;

  virtual views::BubbleAnchor GetAnchor() = 0;

  virtual void SetBubbleOwner(BubbleOwnerDelegate* owner) = 0;
};
#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_CHIP_INTERFACE_H_
