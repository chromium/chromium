// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/constrained_window/native_web_contents_modal_dialog_manager_views.h"

#include <memory>

#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/non_client_view.h"

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_modality_controller.h"
#endif

using web_modal::SingleWebContentsDialogManager;
using web_modal::SingleWebContentsDialogManagerDelegate;
using web_modal::WebContentsModalDialogHost;
using web_modal::ModalDialogHostObserver;

namespace constrained_window {

NativeWebContentsModalDialogManagerViews::
    NativeWebContentsModalDialogManagerViews(
        gfx::NativeWindow dialog,
        SingleWebContentsDialogManagerDelegate* native_delegate)
    : native_delegate_(native_delegate), dialog_(dialog) {
  ManageDialog();
}

NativeWebContentsModalDialogManagerViews::
    ~NativeWebContentsModalDialogManagerViews() {
  if (host_)
    host_->RemoveObserver(this);

  for (views::Widget* widget : observed_widgets_) {
    widget->RemoveObserver(this);
  }
  CHECK(!IsInObserverList());
}

void NativeWebContentsModalDialogManagerViews::ManageDialog() {
  views::Widget* widget = GetWidget(dialog());
  widget->AddObserver(this);
  observed_widgets_.insert(widget);
  widget->set_movement_disabled(true);

#if defined(USE_AURA)
  // TODO(wittman): remove once the new visual style is complete
  widget->GetNativeWindow()->SetProperty(aura::client::kConstrainedWindowKey,
                                         true);

  wm::SetWindowVisibilityAnimationType(
      widget->GetNativeWindow(), wm::WINDOW_VISIBILITY_ANIMATION_TYPE_ROTATE);

  gfx::NativeView parent = widget->GetNativeView()->parent();
  wm::SetChildWindowVisibilityChangesAnimated(parent);
  // No animations should get performed on the window since that will re-order
  // the window stack which will then cause many problems.
  if (parent->parent()) {
    parent->parent()->SetProperty(aura::client::kAnimationsDisabledKey, true);
  }

  wm::SetModalParent(widget->GetNativeWindow(),
                     native_delegate_->GetWebContents()->GetNativeView());
#endif
}

// SingleWebContentsDialogManager:

bool NativeWebContentsModalDialogManagerViews::IsActive() const {
  return GetWidget(dialog_)->IsActive();
}

void NativeWebContentsModalDialogManagerViews::Show() {
  // The host destroying means the dialogs will be destroyed in short order.
  // Avoid showing dialogs at this point as the necessary native window
  // services may not be present.
  if (host_destroying_)
    return;

  views::Widget* widget = GetWidget(dialog());
#if defined(USE_AURA)
  std::unique_ptr<wm::SuspendChildWindowVisibilityAnimations> suspend;
  if (shown_widgets_.find(widget) != shown_widgets_.end()) {
    suspend = std::make_unique<wm::SuspendChildWindowVisibilityAnimations>(
        widget->GetNativeWindow()->parent());
  }
#endif
  CHECK(host_);

  constrained_window::UpdateWebContentsModalDialogPosition(widget, host_);
  if (host_->ShouldActivateDialog()) {
    widget->Show();
    Focus();
  } else {
    widget->ShowInactive();
  }

#if defined(USE_AURA)
  // TODO(pkotwicz): Control the z-order of the constrained dialog via
  // views::kHostViewKey. We will need to ensure that the parent window's
  // shadows are below the constrained dialog in z-order when we do this.
  shown_widgets_.insert(widget);
#endif

#if !defined(USE_AURA)
  // Don't re-animate when switching tabs. Note this is done on Mac only after
  // the initial Show() call above, and then "sticks" for later calls.
  // TODO(tapted): Consolidate this codepath with Aura.
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_HIDE);
#endif
}

void NativeWebContentsModalDialogManagerViews::Hide() {
  views::Widget* widget = GetWidget(dialog());
#if defined(USE_AURA)
  auto suspend = std::make_unique<wm::SuspendChildWindowVisibilityAnimations>(
      widget->GetNativeWindow()->parent());
#endif
  widget->Hide();
}

void NativeWebContentsModalDialogManagerViews::Close() {
  GetWidget(dialog())->Close();
}

void NativeWebContentsModalDialogManagerViews::Focus() {
  views::Widget* widget = GetWidget(dialog());
  if (widget->widget_delegate() &&
      widget->widget_delegate()->GetInitiallyFocusedView())
    widget->widget_delegate()->GetInitiallyFocusedView()->RequestFocus();
#if defined(USE_AURA)
  // We don't necessarily have a RootWindow yet.
  if (widget->GetNativeView()->GetRootWindow())
    widget->GetNativeView()->Focus();
#endif
}

void NativeWebContentsModalDialogManagerViews::Pulse() {}

// web_modal::ModalDialogHostObserver:

void NativeWebContentsModalDialogManagerViews::OnPositionRequiresUpdate() {
  DCHECK(host_);

  for (views::Widget* widget : observed_widgets_) {
    constrained_window::UpdateWebContentsModalDialogPosition(widget, host_);
  }
}

void NativeWebContentsModalDialogManagerViews::OnHostDestroying() {
  host_->RemoveObserver(this);
  host_ = nullptr;
  host_destroying_ = true;
}

// views::WidgetObserver:

void NativeWebContentsModalDialogManagerViews::OnWidgetClosing(
    views::Widget* widget) {
  WidgetClosing(widget);
}

void NativeWebContentsModalDialogManagerViews::OnWidgetDestroying(
    views::Widget* widget) {
  WidgetClosing(widget);
}

void NativeWebContentsModalDialogManagerViews::HostChanged(
    WebContentsModalDialogHost* new_host) {
  if (host_)
    host_->RemoveObserver(this);

  host_ = new_host;

  // |host_| may be null during WebContents destruction.
  if (host_) {
    host_->AddObserver(this);

    for (views::Widget* widget : observed_widgets_) {
      views::Widget::ReparentNativeView(widget->GetNativeView(),
                                        host_->GetHostView());
    }

    OnPositionRequiresUpdate();
  }
}

gfx::NativeWindow NativeWebContentsModalDialogManagerViews::dialog() {
  return dialog_;
}

views::Widget* NativeWebContentsModalDialogManagerViews::GetWidget(
    gfx::NativeWindow dialog) {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(dialog);
  DCHECK(widget);
  return widget;
}

void NativeWebContentsModalDialogManagerViews::WidgetClosing(
    views::Widget* widget) {
#if defined(USE_AURA)
  gfx::NativeView view = widget->GetNativeView()->parent();
  // Allow the parent to animate again.
  if (view && view->parent())
    view->parent()->ClearProperty(aura::client::kAnimationsDisabledKey);
#endif
  widget->RemoveObserver(this);
  observed_widgets_.erase(widget);

#if defined(USE_AURA)
  shown_widgets_.erase(widget);
#endif

  // Will cause this object to be deleted.
  native_delegate_->WillClose(widget->GetNativeWindow());
}

}  // namespace constrained_window
