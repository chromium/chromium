// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/view_visible_in_active_widget_notifier.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

namespace web_app {

base::WeakPtr<ViewVisibleInActiveWidgetNotifier>
ViewVisibleInActiveWidgetNotifier::Create(
    views::Widget* widget,
    ui::ElementIdentifier element_identifier,
    Callback callback) {
  ViewVisibleInActiveWidgetNotifier* notifier =
      new ViewVisibleInActiveWidgetNotifier(widget, element_identifier,
                                            std::move(callback));
  return notifier->weak_ptr_factory_.GetWeakPtr();
}

ViewVisibleInActiveWidgetNotifier::~ViewVisibleInActiveWidgetNotifier() =
    default;

ViewVisibleInActiveWidgetNotifier::ViewVisibleInActiveWidgetNotifier(
    views::Widget* widget,
    ui::ElementIdentifier element_identifier,
    Callback callback)
    : widget_(widget),
      element_identifier_(element_identifier),
      callback_(std::move(callback)) {
  CHECK(widget_);
  CHECK(element_identifier_);
  element_context_ = views::ElementTrackerViews::GetContextForWidget(widget_);
  CHECK(element_context_);

  // This always needs to observe the widget even if the notification is
  // fired in case the widget is destroyed before the posted call to
  // `RunCallback()`.
  widget_observation_.Observe(widget_);

  // If the widget is active and the element is shown, immediately queue the
  // callback without adding observers.
  if (!MaybePostCallback()) {
    // If the surface is not ready, then the callback_ was not posted, so
    // subscribe to changes in the widget and the element tracker.
    element_shown_subscription_ =
        ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
            element_identifier_, element_context_,
            base::BindRepeating(
                &ViewVisibleInActiveWidgetNotifier::OnElementShown,
                weak_ptr_factory_.GetWeakPtr()));

    paint_as_active_subscription_ =
        widget_->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
            &ViewVisibleInActiveWidgetNotifier::OnWidgetPaintedAsActive,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void ViewVisibleInActiveWidgetNotifier::OnWidgetDestroying(
    views::Widget* widget) {
  RunCallback(/*conditions_met=*/false);
}

void ViewVisibleInActiveWidgetNotifier::OnWidgetPaintedAsActive() {
  MaybePostCallback();
}

void ViewVisibleInActiveWidgetNotifier::OnElementShown(
    ui::TrackedElement* element_shown) {
  MaybePostCallback();
}

bool ViewVisibleInActiveWidgetNotifier::MaybePostCallback() {
  const bool is_widget_activate = widget_->ShouldPaintAsActive();
  const bool is_element_visible =
      ui::ElementTracker::GetElementTracker()->IsElementVisible(
          element_identifier_, element_context_);
  if (!is_widget_activate || !is_element_visible) {
    return false;
  }

  // Ensure the widget activation callback posted is asynchronous so that
  // widget destruction in the current message loop are handled gracefully.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ViewVisibleInActiveWidgetNotifier::RunCallback,
                                weak_ptr_factory_.GetWeakPtr(),
                                /*conditions_met=*/true));
  return true;
}

void ViewVisibleInActiveWidgetNotifier::RunCallback(bool conditions_met) {
  if (callback_) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    widget_observation_.Reset();
    std::move(callback_).Run(conditions_met);
    delete this;
  }
}

void PostCallbackOnBrowserActivation(
    const Browser* browser,
    ui::ElementIdentifier id,
    base::OnceCallback<void(bool)> view_and_element_activated_callback) {
  views::Widget* widget =
      BrowserView::GetBrowserViewForBrowser(browser)->GetWidget();
  base::WeakPtr<ViewVisibleInActiveWidgetNotifier> notifier =
      ViewVisibleInActiveWidgetNotifier::Create(
          widget, id, std::move(view_and_element_activated_callback));
}

}  // namespace web_app
