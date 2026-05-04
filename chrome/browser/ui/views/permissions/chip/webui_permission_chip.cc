// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/webui_permission_chip.h"

#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// TODO(crbug.com/502597958): This C++/Mojo type conversion code could be
// simplified with a `ToolbarUIService` mojom_traits file.
toolbar_ui_api::mojom::PermissionChipTheme GetMojoTheme(
    PermissionChipTheme theme) {
  switch (theme) {
    case PermissionChipTheme::kNormalVisibility:
      return toolbar_ui_api::mojom::PermissionChipTheme::kNormalVisibility;
    case PermissionChipTheme::kLowVisibility:
      return toolbar_ui_api::mojom::PermissionChipTheme::kLowVisibility;
    case PermissionChipTheme::kInUseActivityIndicator:
      return toolbar_ui_api::mojom::PermissionChipTheme::
          kInUseActivityIndicator;
    case PermissionChipTheme::kBlockedActivityIndicator:
      return toolbar_ui_api::mojom::PermissionChipTheme::
          kBlockedActivityIndicator;
    case PermissionChipTheme::kOnSystemBlockedActivityIndicator:
      return toolbar_ui_api::mojom::PermissionChipTheme::
          kOnSystemBlockedActivityIndicator;
  }
  NOTREACHED();
}

toolbar_ui_api::mojom::PermissionPromptStyle GetMojoPromptStyle(
    PermissionPromptStyle style) {
  switch (style) {
    case PermissionPromptStyle::kBubbleOnly:
      return toolbar_ui_api::mojom::PermissionPromptStyle::kBubbleOnly;
    case PermissionPromptStyle::kChip:
      return toolbar_ui_api::mojom::PermissionPromptStyle::kChip;
    case PermissionPromptStyle::kLocationBarRightIcon:
      return toolbar_ui_api::mojom::PermissionPromptStyle::
          kLocationBarRightIcon;
    case PermissionPromptStyle::kQuietChip:
      return toolbar_ui_api::mojom::PermissionPromptStyle::kQuietChip;
  }
  NOTREACHED();
}

toolbar_ui_api::mojom::PermissionAction GetMojoPermissionAction(
    permissions::PermissionAction action) {
  switch (action) {
    case permissions::PermissionAction::GRANTED:
      return toolbar_ui_api::mojom::PermissionAction::kGranted;
    case permissions::PermissionAction::DENIED:
      return toolbar_ui_api::mojom::PermissionAction::kDenied;
    case permissions::PermissionAction::DISMISSED:
      return toolbar_ui_api::mojom::PermissionAction::kDismissed;
    case permissions::PermissionAction::IGNORED:
      return toolbar_ui_api::mojom::PermissionAction::kIgnored;
    case permissions::PermissionAction::REVOKED:
      return toolbar_ui_api::mojom::PermissionAction::kRevoked;
    case permissions::PermissionAction::GRANTED_ONCE:
      return toolbar_ui_api::mojom::PermissionAction::kGrantedOnce;
    case permissions::PermissionAction::NUM:
      return toolbar_ui_api::mojom::PermissionAction::kUnspecified;
  }
  NOTREACHED();
}

}  // namespace

WebUIPermissionChip::WebUIPermissionChip(WebUILocationBar* location_bar)
    : location_bar_(location_bar) {}

WebUIPermissionChip::~WebUIPermissionChip() = default;

void WebUIPermissionChip::SetVisible(bool visible) {
  if (is_visible_ == visible) {
    return;
  }
  is_visible_ = visible;
  NotifyVisibilityChanged();
  UpdateState();
}

bool WebUIPermissionChip::GetVisible() const {
  return is_visible_;
}

void WebUIPermissionChip::SetChipIcon(const gfx::VectorIcon& icon) {
  icon_name_ = icon.name;
  UpdateState();
}

void WebUIPermissionChip::SetChipIcon(const gfx::VectorIcon* icon) {
  if (icon) {
    icon_name_ = icon->name;
    UpdateState();
  }
}

void WebUIPermissionChip::SetMessage(std::u16string message) {
  message_ = message;
  UpdateState();
}

void WebUIPermissionChip::SetTooltipText(const std::u16string& tooltip) {
  tooltip_ = tooltip;
  UpdateState();
}

void WebUIPermissionChip::SetTheme(PermissionChipTheme theme) {
  theme_ = theme;
  UpdateState();
}

void WebUIPermissionChip::SetUserDecision(
    permissions::PermissionAction user_decision) {
  user_decision_ = user_decision;
  UpdateState();
}

void WebUIPermissionChip::SetBlockedIconShowing(bool should_show_blocked_icon) {
  should_show_blocked_icon_ = should_show_blocked_icon;
  UpdateState();
}

void WebUIPermissionChip::SetPermissionPromptStyle(
    PermissionPromptStyle prompt_style) {
  prompt_style_ = prompt_style;
  UpdateState();
}

void WebUIPermissionChip::AnimateCollapse(base::TimeDelta duration) {
  // Mirror Native Views' gfx::SlideAnimation::BeginAnimating() behavior:
  // if the animation is already targeting this state, do nothing. This prevents
  // us from hanging indefinitely on `is_animating_` since the frontend won't
  // fire an IPC for a redundant DOM state.
  if (should_collapse_) {
    return;
  }
  is_animating_ = true;
  should_collapse_ = true;
  UpdateState();
}

void WebUIPermissionChip::AnimateExpand(base::TimeDelta duration) {
  // Mirror Native Views' gfx::SlideAnimation::BeginAnimating() behavior:
  // if the animation is already targeting this state, do nothing. This prevents
  // us from hanging indefinitely on `is_animating_` since the frontend won't
  // fire an IPC for a redundant DOM state.
  if (!should_collapse_) {
    return;
  }
  is_animating_ = true;
  is_fully_collapsed_ = false;
  should_collapse_ = false;
  UpdateState();
}

void WebUIPermissionChip::AnimateToFit(base::TimeDelta duration) {
  AnimateExpand(duration);
}

void WebUIPermissionChip::ResetAnimation(AnimationState state) {
  bool was_animating = is_animating_;
  is_animating_ = false;

  // Instantly snap the C++ backend to the requested state.
  is_fully_collapsed_ = (state == AnimationState::kCollapsed);

  // Synchronize the Mojo target state with the C++ backend state.
  // Note: This triggers a DOM update on the frontend. We must not rely on the
  // frontend's subsequent asynchronous IPC to notify observers because C++
  // callers (e.g., ChipController) expect ResetAnimation to be fully
  // synchronous. The `is_animating_ = false` assignment above acts as a
  // firewall, ensuring we safely drop the WebUI's late, redundant IPC.
  should_collapse_ = is_fully_collapsed_;

  // In Native Views, gfx::Animation::Reset() calls Stop(), which synchronously
  // fires AnimationEnded() if the animation was currently running. We must
  // mirror that synchronous callback here.
  if (was_animating) {
    if (is_fully_collapsed_) {
      observers_.Notify(&Observer::OnCollapseAnimationEnded);
    } else {
      observers_.Notify(&Observer::OnExpandAnimationEnded);
    }
  }

  UpdateState();
}

bool WebUIPermissionChip::IsFullyCollapsed() const {
  return is_fully_collapsed_;
}

bool WebUIPermissionChip::IsAnimating() const {
  return is_animating_;
}

void WebUIPermissionChip::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WebUIPermissionChip::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::CallbackListSubscription WebUIPermissionChip::AddVisibilityCallback(
    base::RepeatingClosure callback) {
  return visibility_callbacks_.Add(std::move(callback));
}

void WebUIPermissionChip::SetAccessibilityIgnored(bool is_ignored) {
  // No-op for WebUI. When the chip is hidden in WebUI, it is removed from the
  // DOM entirely (via Lit's conditional rendering), meaning it naturally drops
  // out of the accessibility tree without needing explicit aria-hidden flags.
}

void WebUIPermissionChip::SetAccessibilityName(const std::u16string& name) {
  accessibility_name_ = name;
  UpdateState();
}

void WebUIPermissionChip::AnnounceText(const std::u16string& text) {
  // TODO(crbug.com/495419742): Implement this by adding a Mojo IPC to let WebUI
  // use cr-a11y-announcer.
}

void WebUIPermissionChip::AnnounceAlert(const std::u16string& text) {
  // TODO(crbug.com/495419742): Implement this by adding a Mojo IPC to let WebUI
  // use cr-a11y-announcer.
}

bool WebUIPermissionChip::IsMouseHovered() const {
  return is_mouse_hovered_;
}

void WebUIPermissionChip::SetPressedCallback(base::RepeatingClosure callback) {
  pressed_callback_ = std::move(callback);
}

views::BubbleAnchor WebUIPermissionChip::GetAnchor() {
  // The WebUI element tracker registration happens asynchronously over Mojo.
  // If a permission is requested during browser startup, we might attempt to
  // anchor the bubble before the WebUI has finished registering the tracked
  // element, causing GetAnchorOrNull() to return nullptr. We fallback to the
  // main window contents view to prevent a crash during this sub-millisecond
  // race condition.
  if (ui::TrackedElement* element = location_bar_->GetAnchorOrNull()) {
    return views::BubbleAnchor(element);
  }
  return views::BubbleAnchor(
      location_bar_->GetLocationBarWidget()->GetContentsView());
}

void WebUIPermissionChip::SetBubbleOwner(BubbleOwnerDelegate* owner) {
  // TODO(crbug.com/495419742): Track bubble ownership to manage the active UI
  // state (e.g. highlight) of the chip in the frontend.
}

void WebUIPermissionChip::OnExpandAnimationEnded() {
  // Ignore blind IPCs sent by the WebUI frontend after a forced synchronous
  // snap triggered by ResetAnimation().
  if (!is_animating_) {
    return;
  }
  // Ignore stale IPCs if a new animation or reset was triggered before the
  // frontend finished processing the previous one.
  if (should_collapse_) {
    return;
  }
  is_animating_ = false;
  is_fully_collapsed_ = false;
  observers_.Notify(&Observer::OnExpandAnimationEnded);
}

void WebUIPermissionChip::OnCollapseAnimationEnded() {
  // Ignore blind IPCs sent by the WebUI frontend after a forced synchronous
  // snap triggered by ResetAnimation().
  if (!is_animating_) {
    return;
  }
  // Ignore stale IPCs if a new animation or reset was triggered before the
  // frontend finished processing the previous one.
  if (!should_collapse_) {
    return;
  }
  is_animating_ = false;
  is_fully_collapsed_ = true;
  observers_.Notify(&Observer::OnCollapseAnimationEnded);
}

void WebUIPermissionChip::OnMousePressed() {
  observers_.Notify(&Observer::OnMousePressed);
}

void WebUIPermissionChip::OnClicked() {
  if (pressed_callback_) {
    pressed_callback_.Run();
  }
}

void WebUIPermissionChip::OnMouseEntered() {
  is_mouse_hovered_ = true;
}

void WebUIPermissionChip::OnMouseExited() {
  is_mouse_hovered_ = false;
}

toolbar_ui_api::mojom::PermissionChipStatePtr WebUIPermissionChip::GetState()
    const {
  auto state = toolbar_ui_api::mojom::PermissionChipState::New();
  state->is_visible = is_visible_;
  state->icon_name = icon_name_;
  state->message = message_;
  state->tooltip = tooltip_;
  state->theme = GetMojoTheme(theme_);
  state->user_decision = GetMojoPermissionAction(user_decision_);
  state->should_show_blocked_icon = should_show_blocked_icon_;
  state->prompt_style = GetMojoPromptStyle(prompt_style_);
  // In a declarative WebUI architecture, the Mojo state represents the target
  // state that triggers the browser's CSS animation engine. We serialize
  // `should_collapse_` (the target state) rather than `is_fully_collapsed_`
  // (the actual C++ state), because sending the target state is what commands
  // the frontend to begin its CSS transition.
  state->is_fully_collapsed = should_collapse_;
  state->accessibility_name = accessibility_name_;
  return state;
}

void WebUIPermissionChip::NotifyVisibilityChanged() {
  observers_.Notify(&Observer::OnChipVisibilityChanged, is_visible_);
  visibility_callbacks_.Notify();
}

void WebUIPermissionChip::UpdateState() {
  location_bar_->OnChanged();
}
