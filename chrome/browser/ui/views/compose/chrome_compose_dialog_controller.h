// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMPOSE_CHROME_COMPOSE_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_COMPOSE_CHROME_COMPOSE_DIALOG_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/compose/compose_dialog_view.h"
#include "chrome/browser/ui/webui/compose/compose_ui.h"
#include "components/compose/core/browser/compose_dialog_controller.h"
#include "components/strings/grit/components_strings.h"
#include "components/zoom/zoom_controller.h"
#include "components/zoom/zoom_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class RectF;
}

// Controls how Compose dialogs are shown and hidden, and animations related to
// both actions.
class ChromeComposeDialogController : public compose::ComposeDialogController,
                                      views::WidgetObserver,
                                      zoom::ZoomObserver {
 public:
  explicit ChromeComposeDialogController(content::WebContents* contents);
  ~ChromeComposeDialogController() override;

  // Create and show the dialog view.
  void ShowComposeDialog(views::View* anchor_view,
                         const gfx::RectF& element_bounds_in_screen);

  // Returns the currently shown compose dialog. Returns nullptr if the dialog
  // is not currently shown.
  BubbleContentsWrapperT<ComposeUI>* GetBubbleWrapper() const;

  // Shows the current dialog view, if there is one.
  void ShowUI() override;

  void Close() override;

  bool IsDialogShowing() override;

  // views::WidgetObserver implementation.
  // Invoked when `widget` changes bounds.
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // views::WidgetObserver implementation.
  // The destroying event occurs immediately before the widget is destroyed.
  void OnWidgetDestroying(views::Widget* widget) override;

  // zoom::ZoomObserver implementation.
  // Notification that the zoom percentage has changed.
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;

  // zoom::ZoomObserver implementation.
  // Fired when the ZoomController is destructed.
  void OnZoomControllerDestroyed(
      zoom::ZoomController* zoom_controller) override;

 private:
  friend class ChromeComposeDialogControllerTest;

  base::WeakPtr<ComposeDialogView> bubble_;
  base::WeakPtr<content::WebContents> web_contents_;

  // Observer for the parent widget.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  // Observer for the zoom controller.
  base::ScopedObservation<zoom::ZoomController, zoom::ZoomObserver>
      zoom_observation_{this};

  base::WeakPtrFactory<ChromeComposeDialogController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMPOSE_CHROME_COMPOSE_DIALOG_CONTROLLER_H_
