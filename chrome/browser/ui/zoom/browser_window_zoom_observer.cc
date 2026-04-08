// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/zoom/browser_window_zoom_observer.h"

#include "chrome/browser/ui/browser_command_controller.h"  // nogncheck
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/zoom/zoom_controller.h"

DEFINE_USER_DATA(BrowserWindowZoomObserver);

BrowserWindowZoomObserver::BrowserWindowZoomObserver(
    BrowserWindowInterface* browser)
    : browser_(browser),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {
  // TODO(crbug.com/452120900): TabStripModel auto-unregistered by dtor
  browser_->GetTabStripModel()->AddObserver(this);
}

BrowserWindowZoomObserver::~BrowserWindowZoomObserver() = default;

// static
BrowserWindowZoomObserver* BrowserWindowZoomObserver::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

base::CallbackListSubscription
BrowserWindowZoomObserver::RegisterZoomChangedCallback(
    ZoomChangedCallback callback) {
  return zoom_changed_callbacks_.Add(std::move(callback));
}

void BrowserWindowZoomObserver::OnZoomControllerDestroyed(
    zoom::ZoomController* zoom_controller) {
  if (zoom_observations_.IsObservingSource(zoom_controller)) {
    zoom_observations_.RemoveObservation(zoom_controller);
  }
}

void BrowserWindowZoomObserver::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  if (data.web_contents ==
      browser_->GetTabStripModel()->GetActiveWebContents()) {
    zoom_changed_callbacks_.Notify(data.can_show_bubble);
    // Update zoom commands state (zoom in/out/reset enabled/disabled).
    browser_->GetFeatures().browser_command_controller()->ZoomStateChanged();
  }
}

void BrowserWindowZoomObserver::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents) {
      zoom::ZoomController* zoom_controller =
          zoom::ZoomController::FromWebContents(contents.contents);
      if (zoom_controller) {
        zoom_observations_.AddObservation(zoom_controller);
      }
    }
  } else if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& contents : change.GetRemove()->contents) {
      zoom::ZoomController* zoom_controller =
          zoom::ZoomController::FromWebContents(contents.contents);
      if (zoom_controller &&
          zoom_observations_.IsObservingSource(zoom_controller)) {
        zoom_observations_.RemoveObservation(zoom_controller);
      }
    }
  } else if (change.type() == TabStripModelChange::kReplaced) {
    auto* const replace = change.GetReplace();
    zoom::ZoomController* old_zoom_controller =
        zoom::ZoomController::FromWebContents(replace->old_contents);
    if (old_zoom_controller &&
        zoom_observations_.IsObservingSource(old_zoom_controller)) {
      zoom_observations_.RemoveObservation(old_zoom_controller);
    }
    zoom::ZoomController* new_zoom_controller =
        zoom::ZoomController::FromWebContents(replace->new_contents);
    if (new_zoom_controller) {
      zoom_observations_.AddObservation(new_zoom_controller);
    }
  }

  // Refresh zoom UI when the active tab changes.
  if (selection.active_tab_changed() && selection.new_contents) {
    zoom_changed_callbacks_.Notify(/*can_show_bubble=*/false);
  }
}
