// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_COORDINATOR_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/widget/widget_observer.h"

class ZoomBubbleView;
class BrowserWindowInterface;

namespace content {
class WebContents;
}

// Manages the zoom bubble, ensuring that only one is shown at a time and that
// it is properly cleared. This class is responsible for creating, showing,
// hiding, and refreshing the ZoomBubbleView.
class ZoomBubbleCoordinator : public views::WidgetObserver,
                              public ImmersiveModeController::Observer {
 public:
  DECLARE_USER_DATA(ZoomBubbleCoordinator);

  explicit ZoomBubbleCoordinator(BrowserView& browser_view);
  ZoomBubbleCoordinator(const ZoomBubbleCoordinator&) = delete;
  ZoomBubbleCoordinator& operator=(const ZoomBubbleCoordinator&) = delete;
  ~ZoomBubbleCoordinator() override;

  // Retrieves from the a browser window interface, or null if none.
  // Note: May return null in unit_tests, even for a valid `browser`.
  static ZoomBubbleCoordinator* From(BrowserWindowInterface* browser);

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // Creates and shows a new zoom bubble for the given `contents`, hiding any
  // existing bubble first.
  void Show(content::WebContents* contents,
            LocationBarBubbleDelegateView::DisplayReason reason);

  // Hides the currently showing zoom bubble, if one exists.
  // NOTE: This is async, as a result, the hide is not immediate. Callers should
  // ensure to wait to widget destruction.
  void Hide();

  // Refreshes the existing bubble if it's already showing for `contents`.
  // Returns true if a bubble was successfully refreshed, false otherwise.
  bool RefreshIfShowing(content::WebContents* contents);

  // Returns true if a zoom bubble is currently being shown.
  [[nodiscard]] bool IsShowing() const {
    return widget_observation_.IsObserving() &&
           !widget_observation_.GetSource()->IsClosed();
  }

  // Returns a pointer to the currently showing bubble, or nullptr if none.
  ZoomBubbleView* bubble();

 private:
  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveModeControllerDestroyed() override;

  // Determines if the existing zoom bubble can be refreshed for the given
  // WebContents.
  bool CanRefresh(ZoomBubbleView* current_bubble,
                  content::WebContents* web_contents);

  // Updates visibility of the zoom icon.
  void UpdateZoomBubbleStateAndIconVisibility(bool is_bubble_visible);

  ui::ScopedUnownedUserData<ZoomBubbleCoordinator> scoped_unowned_user_data_;

  // Unowned reference to the  browser view that whole this coordinator.
  const raw_ref<BrowserView> browser_view_;

  // Observes the widget of the zoom bubble to be notified of its destruction.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  // The immersive mode controller observation for the BrowserView containing
  // `web_contents_`.
  base::ScopedObservation<ImmersiveModeController,
                          ImmersiveModeController::Observer>
      immersive_mode_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_COORDINATOR_H_
