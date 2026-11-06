// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ZOOM_BROWSER_WINDOW_ZOOM_OBSERVER_H_
#define CHROME_BROWSER_UI_ZOOM_BROWSER_WINDOW_ZOOM_OBSERVER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/zoom/zoom_controller.h"
#include "components/zoom/zoom_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

// Window-scoped feature, owned by BrowserWindowFeatures, that bridges per-tab
// zoom changes into listener-style callbacks.
//
// Rather than having every window-level UI component track the active tab and
// inherit zoom::ZoomObserver, this class follows the window's TabStripModel,
// observes the zoom::ZoomController of every tab in the window, and notifies
// subscribers registered via RegisterZoomChangedCallback() when zoom changes
// on the *active* tab (or the active tab itself changes). Subscriptions are
// RAII-managed: destroying the returned base::CallbackListSubscription
// unregisters the callback, so subscribers need no manual teardown.
//
// Also keeps zoom command state (zoom in/out/reset) up to date via
// BrowserCommandController.
//
// All methods must be called on the UI thread.
class BrowserWindowZoomObserver : public zoom::ZoomObserver,
                                  public TabStripModelObserver {
 public:
  DECLARE_USER_DATA(BrowserWindowZoomObserver);

  // Invoked when zoom changes on the active tab. `can_show_bubble` indicates
  // whether the change may trigger the zoom notification bubble.
  using ZoomChangedCallback = base::RepeatingCallback<void(bool)>;

  explicit BrowserWindowZoomObserver(BrowserWindowInterface* browser);
  BrowserWindowZoomObserver(const BrowserWindowZoomObserver&) = delete;
  BrowserWindowZoomObserver& operator=(const BrowserWindowZoomObserver&) =
      delete;
  ~BrowserWindowZoomObserver() override;

  // Returns the instance attached to `browser`, or nullptr if none.
  static BrowserWindowZoomObserver* From(BrowserWindowInterface* browser);
  static const BrowserWindowZoomObserver* From(
      const BrowserWindowInterface* browser);

  // Registers `callback` to be invoked when zoom changes on the active tab.
  // The callback is invoked until the returned subscription is destroyed.
  base::CallbackListSubscription RegisterZoomChangedCallback(
      ZoomChangedCallback callback);

  // Returns true if `zoom_controller` is currently being observed.
  bool IsObservingZoomControllerForTesting(
      zoom::ZoomController& zoom_controller) const;

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
  void NotifyZoomChanged(bool can_show_bubble);

  const raw_ptr<BrowserWindowInterface> browser_;
  base::RepeatingCallbackList<void(bool)> zoom_changed_callbacks_;
  base::ScopedMultiSourceObservation<zoom::ZoomController, zoom::ZoomObserver>
      zoom_observations_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  ui::ScopedUnownedUserData<BrowserWindowZoomObserver>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_ZOOM_BROWSER_WINDOW_ZOOM_OBSERVER_H_
