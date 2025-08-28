// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_SYNC_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_SYNC_IMPL_H_

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_experiment_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"

namespace tabs_api {

class TabStripServiceSyncImpl : public TabStripService {
 public:
  TabStripServiceSyncImpl(BrowserWindowInterface* browser,
                          TabStripModel* tab_strip_model);

  TabStripServiceSyncImpl(
      std::unique_ptr<BrowserAdapter> browser_adapter,
      std::unique_ptr<TabStripModelAdapter> tab_strip_model_adapter);

  ~TabStripServiceSyncImpl() override;

  TabStripService::GetTabsResult GetTabs() override;
  mojom::TabStripService::GetTabResult GetTab(
      const tabs_api::NodeId& id) override;
  mojom::TabStripService::CreateTabAtResult CreateTabAt(
      const std::optional<tabs_api::Position>& pos,
      const std::optional<GURL>& url) override;
  mojom::TabStripService::CloseTabsResult CloseTabs(
      const std::vector<tabs_api::NodeId>& ids) override;
  mojom::TabStripService::ActivateTabResult ActivateTab(
      const tabs_api::NodeId& id) override;
  mojom::TabStripService::SetSelectedTabsResult SetSelectedTabs(
      const std::vector<tabs_api::NodeId>& selection,
      const tabs_api::NodeId& tab_to_activate) override;
  mojom::TabStripService::MoveTabResult MoveTab(
      const tabs_api::NodeId& id,
      const tabs_api::Position& position) override;

  // tabs_api::mojom::TabStripExperimentalService overrides
  //
  // TabStripExperimentalService is intended for quick prototyping for
  // experimental apis that may not necessarily fit in the standard
  // TabStripService.
  mojom::TabStripExperimentService::UpdateTabGroupVisualResult
  UpdateTabGroupVisual(
      const tabs_api::NodeId& id,
      const tab_groups::TabGroupVisualData& visual_data) override;

  void AddObserver(TabStripModelObserver* observer) override;
  void RemoveObserver(TabStripModelObserver* observer) override;

 private:
  std::unique_ptr<tabs_api::BrowserAdapter> browser_adapter_;
  std::unique_ptr<tabs_api::TabStripModelAdapter> tab_strip_model_adapter_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_SYNC_IMPL_H_
