// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_VIEW_VISIBLE_IN_ACTIVE_WIDGET_NOTIFIER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_VIEW_VISIBLE_IN_ACTIVE_WIDGET_NOTIFIER_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace web_app {

class ViewVisibleInActiveWidgetNotifier : public views::WidgetObserver {
 public:
  using Callback = base::OnceCallback<void(bool conditions_met)>;
  // A fire-and-forget utility that runs `callback` when `widget` is active (or
  // "paint-as-active"), and the specified element in the widget is visible. If
  // those conditions are never met, the callback is called on `widget`
  // destruction. The notifier deletes itself after invoking the callback.
  static base::WeakPtr<ViewVisibleInActiveWidgetNotifier> Create(
      views::Widget* widget,
      ui::ElementIdentifier element_identifier,
      Callback callback);

  ~ViewVisibleInActiveWidgetNotifier() override;
  ViewVisibleInActiveWidgetNotifier(const ViewVisibleInActiveWidgetNotifier&) =
      delete;
  void operator=(const ViewVisibleInActiveWidgetNotifier&) = delete;

 private:
  // Creates the notifier and calls `MaybePostCallback()` in case the conditions
  // are already met; otherwise, subscribes to the relevant observations.
  ViewVisibleInActiveWidgetNotifier(views::Widget* widget,
                                    ui::ElementIdentifier element_identifier,
                                    Callback callback);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Widget paint as active observers.
  void OnWidgetPaintedAsActive();

  // ui::ElementTracker listeners:
  void OnElementShown(ui::TrackedElement* element_shown);

  bool MaybePostCallback();
  void RunCallback(bool conditions_met);

  const raw_ptr<views::Widget> widget_;
  const ui::ElementIdentifier element_identifier_;
  ui::ElementContext element_context_;
  Callback callback_;

  base::CallbackListSubscription paint_as_active_subscription_;
  base::CallbackListSubscription element_shown_subscription_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::WeakPtrFactory<ViewVisibleInActiveWidgetNotifier> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_VIEW_VISIBLE_IN_ACTIVE_WIDGET_NOTIFIER_H_
