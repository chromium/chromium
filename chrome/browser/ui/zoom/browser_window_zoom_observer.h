// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ZOOM_BROWSER_WINDOW_ZOOM_OBSERVER_H_
#define CHROME_BROWSER_UI_ZOOM_BROWSER_WINDOW_ZOOM_OBSERVER_H_

#include "base/callback_list.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/zoom/zoom_controller.h"
#include "components/zoom/zoom_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

// Observes per-tab ZoomController changes and notifies registered listeners.
// Also updates zoom command state via BrowserCommandController.
class BrowserWindowZoomObserver : public zoom::ZoomObserver,
                                  public TabStripModelObserver {
 public:
  DECLARE_USER_DATA(BrowserWindowZoomObserver);

  using ZoomChangedCallback = base::RepeatingCallback<void(bool)>;

  explicit BrowserWindowZoomObserver(BrowserWindowInterface* browser);
  BrowserWindowZoomObserver(const BrowserWindowZoomObserver&) = delete;
  BrowserWindowZoomObserver& operator=(const BrowserWindowZoomObserver&) =
      delete;
  ~BrowserWindowZoomObserver() override;

  static BrowserWindowZoomObserver* From(BrowserWindowInterface* browser);

  // Register a callback invoked when zoom changes on the active tab.
  // The boolean parameter indicates whether changes to zoom level can trigger
  // the zoom notification bubble.
  base::CallbackListSubscription RegisterZoomChangedCallback(
      ZoomChangedCallback callback);

  // zoom::ZoomObserver:
  void OnZoomControllerDestroyed(
      zoom::ZoomController* zoom_controller) override;
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  const raw_ptr<BrowserWindowInterface> browser_;
  base::RepeatingCallbackList<void(bool)> zoom_changed_callbacks_;
  base::ScopedMultiSourceObservation<zoom::ZoomController, zoom::ZoomObserver>
      zoom_observations_{this};
  ui::ScopedUnownedUserData<BrowserWindowZoomObserver>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_ZOOM_BROWSER_WINDOW_ZOOM_OBSERVER_H_
