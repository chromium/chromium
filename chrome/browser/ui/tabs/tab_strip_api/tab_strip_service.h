// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_H_

#include <optional>
#include <vector>

#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_experiment_api.mojom.h"

class GURL;

namespace tab_groups {

class TabGroupVisualData;

}  // namespace tab_groups

namespace tabs_api {

class NodeId;
class Position;

namespace observation {

class TabStripApiBatchedObserver;

}  // namespace observation

class TabStripService {
 public:
  virtual ~TabStripService() = default;

  using GetTabsResult =
      base::expected<mojom::ContainerPtr, mojo_base::mojom::ErrorPtr>;
  virtual GetTabsResult GetTabs() = 0;
  virtual mojom::TabStripService::GetTabResult GetTab(
      const tabs_api::NodeId& id) = 0;
  virtual mojom::TabStripService::CreateTabAtResult CreateTabAt(
      const std::optional<tabs_api::Position>& pos,
      const std::optional<GURL>& url) = 0;
  virtual mojom::TabStripService::CloseTabsResult CloseTabs(
      const std::vector<tabs_api::NodeId>& ids) = 0;
  virtual mojom::TabStripService::ActivateTabResult ActivateTab(
      const tabs_api::NodeId& id) = 0;
  virtual mojom::TabStripService::SetSelectedTabsResult SetSelectedTabs(
      const std::vector<tabs_api::NodeId>& selection,
      const tabs_api::NodeId& tab_to_activate) = 0;
  virtual mojom::TabStripService::MoveNodeResult MoveNode(
      const tabs_api::NodeId& id,
      const tabs_api::Position& position) = 0;

  // tabs_api::mojom::TabStripExperimentalService = 0s
  //
  // TabStripExperimentalService is intended for quick prototyping for
  // experimental apis that may not necessarily fit in the standard
  // TabStripService.
  virtual mojom::TabStripExperimentService::UpdateTabGroupVisualResult
  UpdateTabGroupVisual(const tabs_api::NodeId& id,
                       const tab_groups::TabGroupVisualData& visual_data) = 0;

  virtual void AddObserver(
      observation::TabStripApiBatchedObserver* observer) = 0;
  virtual void RemoveObserver(
      observation::TabStripApiBatchedObserver* observer) = 0;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_H_
