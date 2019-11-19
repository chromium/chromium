// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"

#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views_context.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/subtle_notification_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/link.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "ui/base/l10n/l10n_util_win.h"
#endif

ExclusiveAccessBubbleViews::ExclusiveAccessBubbleViews(
    ExclusiveAccessBubbleViewsContext* context,
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type,
    ExclusiveAccessBubbleHideCallback bubble_first_hide_callback)
    : ExclusiveAccessBubble(context->GetExclusiveAccessManager(),
                            url,
                            bubble_type),
      bubble_view_context_(context),
      popup_(nullptr),
      bubble_first_hide_callback_(std::move(bubble_first_hide_callback)),
      animation_(new gfx::SlideAnimation(this)) {
  // Create the contents view.
  ui::Accelerator accelerator(ui::VKEY_UNKNOWN, ui::EF_NONE);
  bool got_accelerator =
      bubble_view_context_->GetAcceleratorProvider()
          ->GetAcceleratorForCommandId(IDC_FULLSCREEN, &accelerator);
  DCHECK(got_accelerator);
  view_ = new SubtleNotificationView();
  browser_fullscreen_exit_accelerator_ = accelerator.GetShortcutText();
  UpdateViewContent(bubble_type_);

  // Initialize the popup.
  popup_ = SubtleNotificationView::CreatePopupWidget(
      bubble_view_context_->GetBubbleParentView(), view_);
  gfx::Size size = GetPopupRect(true).size();
  // Bounds are in screen coordinates.
  popup_->SetBounds(GetPopupRect(false));
  // Why is this special enough to require the "security surface" level? A
  // decision was made a long time ago to not require confirmation when a site
  // asks to go fullscreen, and that's not changing. However, a site going
  // fullscreen is a big security risk, allowing phishing and other UI fakery.
  // This bubble is the only defense that Chromium can provide against this
  // attack, so it's important to order it above everything.
  //
  // On some platforms, pages can put themselves into fullscreen and then
  // trigger other elements to cover up this bubble, elements that aren't fully
  // under Chromium's control. See https://crbug.com/927150 for an example.
  popup_->SetZOrderLevel(ui::ZOrderLevel::kSecuritySurface);
  view_->SetBounds(0, 0, size.width(), size.height());
  popup_->AddObserver(this);

  fullscreen_observer_.Add(bubble_view_context_->GetExclusiveAccessManager()
                               ->fullscreen_controller());

  UpdateMouseWatcher();
}

ExclusiveAccessBubbleViews::~ExclusiveAccessBubbleViews() {
  RunHideCallbackIfNeeded(ExclusiveAccessBubbleHideReason::kInterrupted);

  popup_->RemoveObserver(this);

  // This is tricky.  We may be in an ATL message handler stack, in which case
  // the popup cannot be deleted yet.  We also can't set the popup's ownership
  // model to NATIVE_WIDGET_OWNS_WIDGET because if the user closed the last tab
  // while in fullscreen mode, Windows has already destroyed the popup HWND by
  // the time we get here, and thus either the popup will already have been
  // deleted (if we set this in our constructor) or the popup will never get
  // another OnFinalMessage() call (if not, as currently).  So instead, we tell
  // the popup to synchronously hide, and then asynchronously close and delete
  // itself.
  popup_->Close();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, popup_);
}

void ExclusiveAccessBubbleViews::UpdateContent(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type,
    ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
    bool force_update) {
  DCHECK_NE(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE, bubble_type);
  if (bubble_type_ == bubble_type && url_ == url && !force_update)
    return;

  // Bubble maybe be re-used after timeout.
  RunHideCallbackIfNeeded(ExclusiveAccessBubbleHideReason::kInterrupted);

  bubble_first_hide_callback_ = std::move(bubble_first_hide_callback);

  url_ = url;
  bubble_type_ = bubble_type;
  UpdateViewContent(bubble_type_);

  gfx::Size size = GetPopupRect(true).size();
  view_->SetSize(size);
  popup_->SetBounds(GetPopupRect(false));
  Show();

  // Stop watching the mouse even if UpdateMouseWatcher() will start watching
  // it again so that the popup with the new content is visible for at least
  // |kInitialDelayMs|.
  StopWatchingMouse();

  UpdateMouseWatcher();
}

void ExclusiveAccessBubbleViews::RepositionIfVisible() {
  if (popup_->IsVisible())
    UpdateBounds();
}

void ExclusiveAccessBubbleViews::HideImmediately() {
  if (!popup_->IsVisible())
    return;

  RunHideCallbackIfNeeded(ExclusiveAccessBubbleHideReason::kInterrupted);

  animation_->SetSlideDuration(base::TimeDelta::FromMilliseconds(150));
  animation_->Hide();
}

views::View* ExclusiveAccessBubbleViews::GetView() {
  return view_;
}

void ExclusiveAccessBubbleViews::UpdateMouseWatcher() {
  bool should_watch_mouse = popup_->IsVisible() || CanTriggerOnMouse();

  if (should_watch_mouse == IsWatchingMouse())
    return;

  if (should_watch_mouse)
    StartWatchingMouse();
  else
    StopWatchingMouse();
}

void ExclusiveAccessBubbleViews::UpdateBounds() {
  gfx::Rect popup_rect(GetPopupRect(false));
  if (!popup_rect.IsEmpty()) {
    popup_->SetBounds(popup_rect);
    view_->SetY(popup_rect.height() - view_->height());
  }
}

void ExclusiveAccessBubbleViews::UpdateViewContent(
    ExclusiveAccessBubbleType bubble_type) {
  DCHECK_NE(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE, bubble_type);

  base::string16 accelerator;
  if (bubble_type ==
          EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION ||
      bubble_type ==
          EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION) {
    accelerator = browser_fullscreen_exit_accelerator_;
  } else {
    accelerator = l10n_util::GetStringUTF16(IDS_APP_ESC_KEY);
  }
#if defined(OS_MACOSX)
  // Mac keyboards use lowercase for everything except function keys, which are
  // typically reserved for system use. Since |accelerator| is placed in a box
  // to make it look like a keyboard key it looks weird to not follow suit.
  accelerator = base::i18n::ToLower(accelerator);
#endif
  view_->UpdateContent(GetInstructionText(accelerator));
}

views::View* ExclusiveAccessBubbleViews::GetBrowserRootView() const {
  return bubble_view_context_->GetBubbleAssociatedWidget()->GetRootView();
}

void ExclusiveAccessBubbleViews::AnimationProgressed(
    const gfx::Animation* animation) {
  float opacity = static_cast<float>(animation_->CurrentValueBetween(0.0, 1.0));
  if (opacity == 0) {
    popup_->Hide();
  } else {
    popup_->Show();
    popup_->SetOpacity(opacity);
  }
}

void ExclusiveAccessBubbleViews::AnimationEnded(
    const gfx::Animation* animation) {
  if (animation_->IsShowing())
    GetView()->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  AnimationProgressed(animation);
}

gfx::Rect ExclusiveAccessBubbleViews::GetPopupRect(
    bool ignore_animation_state) const {
  gfx::Size size(view_->GetPreferredSize());
  gfx::Rect widget_bounds = bubble_view_context_->GetClientAreaBoundsInScreen();
  int x = widget_bounds.x() + (widget_bounds.width() - size.width()) / 2;

  int top_container_bottom = widget_bounds.y();
  if (bubble_view_context_->IsImmersiveModeEnabled()) {
    // Skip querying the top container height in non-immersive fullscreen
    // because:
    // - The top container height is always zero in non-immersive fullscreen.
    // - Querying the top container height may return the height before entering
    //   fullscreen because layout is disabled while entering fullscreen.
    // A visual glitch due to the delayed layout is avoided in immersive
    // fullscreen because entering fullscreen starts with the top container
    // revealed. When revealed, the top container has the same height as before
    // entering fullscreen.
    top_container_bottom =
        bubble_view_context_->GetTopContainerBoundsInScreen().bottom();
  }
  // |desired_top| is the top of the bubble area including the shadow.
  int desired_top = kSimplifiedPopupTopPx - view_->border()->GetInsets().top();
  int y = top_container_bottom + desired_top;

  return gfx::Rect(gfx::Point(x, y), size);
}

gfx::Point ExclusiveAccessBubbleViews::GetCursorScreenPoint() {
  return bubble_view_context_->GetCursorPointInParent();
}

bool ExclusiveAccessBubbleViews::WindowContainsPoint(gfx::Point pos) {
  return GetBrowserRootView()->HitTestPoint(pos);
}

bool ExclusiveAccessBubbleViews::IsWindowActive() {
  return bubble_view_context_->GetBubbleAssociatedWidget()->IsActive();
}

void ExclusiveAccessBubbleViews::Hide() {
  // This function is guarded by the |ExclusiveAccessBubble::hide_timeout_|
  // timer, so the bubble has been displayed for at least
  // |ExclusiveAccessBubble::kInitialDelayMs|.
  DCHECK(!IsHideTimeoutRunning());
  RunHideCallbackIfNeeded(ExclusiveAccessBubbleHideReason::kTimeout);

  animation_->SetSlideDuration(base::TimeDelta::FromMilliseconds(700));
  animation_->Hide();
}

void ExclusiveAccessBubbleViews::Show() {
  if (animation_->IsShowing())
    return;
  animation_->SetSlideDuration(base::TimeDelta::FromMilliseconds(350));
  animation_->Show();
}

bool ExclusiveAccessBubbleViews::IsAnimating() {
  return animation_->is_animating();
}

bool ExclusiveAccessBubbleViews::CanTriggerOnMouse() const {
  return bubble_view_context_->CanTriggerOnMouse();
}

void ExclusiveAccessBubbleViews::OnFullscreenStateChanged() {
  UpdateMouseWatcher();
}

void ExclusiveAccessBubbleViews::OnWidgetDestroyed(views::Widget* widget) {
  // Although SubtleNotificationView uses WIDGET_OWNS_NATIVE_WIDGET, a close can
  // originate from the OS or some Chrome shutdown codepaths that bypass the
  // destructor.
  views::Widget* popup_on_stack = popup_;
  DCHECK(popup_on_stack->HasObserver(this));

  // Get ourselves destroyed. Calling ExitExclusiveAccess() won't work because
  // the parent window might be destroyed as well, so asking it to exit
  // fullscreen would be a bad idea.
  bubble_view_context_->DestroyAnyExclusiveAccessBubble();

  // Note: |this| is destroyed on the line above. Check that the destructor was
  // invoked. This is safe to do since |popup_| is deleted via a posted task.
  DCHECK(!popup_on_stack->HasObserver(this));
}

void ExclusiveAccessBubbleViews::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  UpdateMouseWatcher();
}

void ExclusiveAccessBubbleViews::RunHideCallbackIfNeeded(
    ExclusiveAccessBubbleHideReason reason) {
  if (bubble_first_hide_callback_)
    std::move(bubble_first_hide_callback_).Run(reason);
}
