// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dropdown_bar_host.h"

#include <algorithm>

#include "build/build_config.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/dropdown_bar_host_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "ui/compositor/layer.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/widget/widget.h"

// static
bool DropdownBarHost::disable_animations_during_testing_ = false;

////////////////////////////////////////////////////////////////////////////////
// DropdownBarHost, public:

DropdownBarHost::DropdownBarHost(BrowserView* browser_view)
    : AnimationDelegateViews(browser_view), browser_view_(browser_view) {}

DropdownBarHost::~DropdownBarHost() {
  focus_manager_->RemoveFocusChangeListener(this);
  ResetFocusTracker();
}

void DropdownBarHost::Init(views::View* host_view,
                           std::unique_ptr<views::View> view,
                           DropdownBarHostDelegate* delegate) {
  DCHECK(view);
  DCHECK(delegate);

  delegate_ = delegate;

  // The |clip_view| exists to paint to a layer so that it can clip descendent
  // Views which also paint to a Layer. See http://crbug.com/589497
  auto clip_view = std::make_unique<views::View>();
  clip_view->SetPaintToLayer();
  clip_view->layer()->SetFillsBoundsOpaquely(false);
  clip_view->layer()->SetMasksToBounds(true);
  view_ = clip_view->AddChildView(std::move(view));

  // Initialize the host.
  host_ = std::make_unique<ThemeCopyingWidget>(browser_view_->GetWidget());
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.delegate = this;
  params.name = "DropdownBarHost";
  params.parent = browser_view_->GetWidgetForAnchoring()->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
#if BUILDFLAG(IS_MAC)
  params.activatable = views::Widget::InitParams::Activatable::kYes;
#endif
  host_->Init(std::move(params));
  host_->SetContentsView(std::move(clip_view));

  SetHostViewNative(host_view);

  // Start listening to focus changes, so we can register and unregister our
  // own handler for Escape.
  focus_manager_ = host_->GetFocusManager();
  // In some cases (see bug http://crbug.com/17056) it seems we may not have
  // a focus manager.  Please reopen the bug if you hit this.
  CHECK(focus_manager_);
  focus_manager_->AddFocusChangeListener(this);

  animation_ = std::make_unique<gfx::SlideAnimation>(this);
  if (!gfx::Animation::ShouldRenderRichAnimation())
    animation_->SetSlideDuration(base::TimeDelta());

  // Update the widget and |view_| bounds to the hidden state.
  AnimationProgressed(animation_.get());
}

bool DropdownBarHost::IsAnimating() const {
  return animation_->is_animating();
}

bool DropdownBarHost::IsVisible() const {
  return is_visible_;
}

void DropdownBarHost::SetFocusAndSelection() {
  delegate_->FocusAndSelectAll();
}

void DropdownBarHost::StopAnimation() {
  animation_->End();
}

void DropdownBarHost::Show(bool animate) {
  DCHECK(host_);

  if (!focus_tracker_) {
    // Stores the currently focused view, and tracks focus changes so that we
    // can restore focus when the dropdown widget is closed.
    // One may already be set if this Show() call is part of restoring this
    // DropdownBarHost visibility to a specific tab.
    focus_tracker_ =
        std::make_unique<views::ExternalFocusTracker>(view_, focus_manager_);
  }

  SetDialogPosition(GetDialogPosition(gfx::Rect()));

  // If we're in the middle of a close animation, stop it and skip to the end.
  // This ensures that the state is consistent and prepared to show the drop-
  // down bar.
  if (animation_->IsClosing())
    StopAnimation();

  host_->Show();

  bool was_visible = is_visible_;
  is_visible_ = true;
  if (!animate || disable_animations_during_testing_) {
    animation_->Reset(1);
    AnimationProgressed(animation_.get());
  } else if (!was_visible) {
    // Don't re-start the animation.
    animation_->Reset();
    animation_->Show();
  }

  if (!was_visible)
    OnVisibilityChanged();
}

void DropdownBarHost::Hide(bool animate) {
  if (!IsVisible())
    return;

  if (animate && !disable_animations_during_testing_ &&
      !animation_->IsClosing()) {
    animation_->Hide();
  } else {
    if (animation_->IsClosing()) {
      // If we're in the middle of a close animation, skip immediately to the
      // end of the animation.
      StopAnimation();
    } else {
      // Otherwise we need to set both the animation state to ended and the
      // DropdownBarHost state to ended/hidden, otherwise the next time we try
      // to show the bar, it might refuse to do so. Note that we call
      // AnimationEnded ourselves as Reset does not call it if we are not
      // animating here.
      animation_->Reset();
      AnimationEnded(animation_.get());
    }
  }
}

void DropdownBarHost::SetDialogPosition(const gfx::Rect& new_pos) {
  view_->SetSize(new_pos.size());

  if (new_pos.IsEmpty())
    return;

  host()->SetBounds(new_pos);
}

void DropdownBarHost::OnWillChangeFocus(views::View* focused_before,
                                        views::View* focused_now) {
  // First we need to determine if one or both of the views passed in are child
  // views of our view.
  bool our_view_before = focused_before && view_->Contains(focused_before);
  bool our_view_now = focused_now && view_->Contains(focused_now);

  // When both our_view_before and our_view_now are false, it means focus is
  // changing hands elsewhere in the application (and we shouldn't do anything).
  // Similarly, when both are true, focus is changing hands within the dropdown
  // widget (and again, we should not do anything). We therefore only need to
  // look at when we gain initial focus and when we loose it.
  if (!our_view_before && our_view_now) {
    // We are gaining focus from outside the dropdown widget so we must register
    // a handler for Escape.
    RegisterAccelerators();
  } else if (our_view_before && !our_view_now) {
    // We are losing focus to something outside our widget so we restore the
    // original handler for Escape.
    UnregisterAccelerators();
  }
}

void DropdownBarHost::OnDidChangeFocus(views::View* focused_before,
                                       views::View* focused_now) {
}

void DropdownBarHost::AnimationProgressed(const gfx::Animation* animation) {
  // First, we calculate how many pixels to slide the widget.
  gfx::Size pref_size = view_->GetPreferredSize();
  int view_offset = static_cast<int>((animation_->GetCurrentValue() - 1.0) *
                                     pref_size.height());

  // This call makes sure |view_| appears in the right location, the size and
  // shape is correct and that it slides in the right direction.
  view_->SetPosition(gfx::Point(0, view_offset));
}

void DropdownBarHost::AnimationEnded(const gfx::Animation* animation) {
  // Ensure the position gets a final update.  This is important when ending the
  // animation early (e.g. closing a tab with an open find bar), since otherwise
  // the position will be out of date at the start of the next animation.
  AnimationProgressed(animation);

  if (!animation_->IsShowing()) {
    // Animation has finished closing.
    DCHECK(host_);
    host_->Hide();
    is_visible_ = false;
    OnVisibilityChanged();
  } else {
    // Animation has finished opening.
  }
}

void DropdownBarHost::RegisterAccelerators() {
  DCHECK(!esc_accel_target_registered_);
  ui::Accelerator escape(ui::VKEY_ESCAPE, ui::EF_NONE);
  focus_manager_->RegisterAccelerator(
      escape, ui::AcceleratorManager::kNormalPriority, this);
  esc_accel_target_registered_ = true;
}

void DropdownBarHost::UnregisterAccelerators() {
  DCHECK(esc_accel_target_registered_);
  ui::Accelerator escape(ui::VKEY_ESCAPE, ui::EF_NONE);
  focus_manager_->UnregisterAccelerator(escape, this);
  esc_accel_target_registered_ = false;
}

void DropdownBarHost::OnVisibilityChanged() {}

void DropdownBarHost::SetFocusTracker(
    std::unique_ptr<views::ExternalFocusTracker> focus_tracker) {
  focus_tracker_ = std::move(focus_tracker);
}

std::unique_ptr<views::ExternalFocusTracker>
DropdownBarHost::TakeFocusTracker() {
  return std::move(focus_tracker_);
}

void DropdownBarHost::ResetFocusTracker() {
  focus_tracker_.reset();
}

void DropdownBarHost::GetWidgetBounds(gfx::Rect* bounds) {
  DCHECK(bounds);
  *bounds = browser_view_->bounds();
}

views::Widget* DropdownBarHost::GetWidget() {
  return host_.get();
}

const views::Widget* DropdownBarHost::GetWidget() const {
  return host_.get();
}
