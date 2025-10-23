// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_OBSERVATION_TAB_STRIP_API_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_OBSERVATION_TAB_STRIP_API_OBSERVER_H_

#include "chrome/browser/ui/tabs/tab_strip_api/observation/tab_strip_api_batched_observer.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_api_events.mojom.h"

namespace tabs_api::observation {

class TabStripApiObserver : public TabStripApiBatchedObserver {
 public:
  TabStripApiObserver() = default;
  ~TabStripApiObserver() override = default;

  virtual void OnTabsCreated(
      const mojom::OnTabsCreatedEventPtr& tabs_created_event) = 0;
  virtual void OnTabsClosed(
      const mojom::OnTabsClosedEventPtr& tabs_closed_event) = 0;
  virtual void OnNodeMoved(
      const mojom::OnNodeMovedEventPtr& node_moved_event) = 0;
  virtual void OnDataChanged(
      const mojom::OnDataChangedEventPtr& data_changed_event) = 0;
  virtual void OnCollectionCreated(
      const mojom::OnCollectionCreatedEventPtr& collection_created_event) = 0;

  // Dispatch to virtual methods.
  void OnTabEvents(const std::vector<mojom::TabsEventPtr>& events) override;
};

}  // namespace tabs_api::observation

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_OBSERVATION_TAB_STRIP_API_OBSERVER_H_
