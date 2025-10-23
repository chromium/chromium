// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_MOJO_HANDLER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_MOJO_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/observation/tab_strip_api_batched_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_experiment_api.mojom.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class BrowserWindowInterface;
class TabStripModel;

// TODO (crbug.com/409086859). See bug for dd.
// tabs_api::mojom::TabStripController is an experimental TabStrip Api between
// any view and the TabStripModel.
class TabStripServiceMojoHandler
    : public tabs_api::observation::TabStripApiBatchedObserver,
      public tabs_api::mojom::TabStripService,
      public tabs_api::mojom::TabStripExperimentService,
      public TabStripModelObserver,
      public TabStripServiceFeature {
 public:
  TabStripServiceMojoHandler(BrowserWindowInterface* browser,
                             TabStripModel* tab_strip_model);
  TabStripServiceMojoHandler(
      std::unique_ptr<tabs_api::TabStripService> service,
      std::unique_ptr<tabs_api::TabStripModelAdapter> tab_strip_model_adapter);
  TabStripServiceMojoHandler(const TabStripServiceMojoHandler&&) = delete;
  TabStripServiceMojoHandler& operator=(const TabStripServiceMojoHandler&) =
      delete;
  ~TabStripServiceMojoHandler() override;

  // TabStripServiceregister overrides
  void Accept(
      mojo::PendingReceiver<tabs_api::mojom::TabStripService> client) override;
  void AcceptExperimental(
      mojo::PendingReceiver<tabs_api::mojom::TabStripExperimentService> client)
      override;

  // tabs_api::mojom::TabStripService overrides
  void GetTabs(GetTabsCallback callback) override;
  void GetTab(const tabs_api::NodeId& id, GetTabCallback callback) override;
  void CreateTabAt(const std::optional<tabs_api::Position>& pos,
                   const std::optional<GURL>& url,
                   CreateTabAtCallback callback) override;
  void CloseTabs(const std::vector<tabs_api::NodeId>& ids,
                 CloseTabsCallback callback) override;
  void ActivateTab(const tabs_api::NodeId& id,
                   ActivateTabCallback callback) override;
  void SetSelectedTabs(const std::vector<tabs_api::NodeId>& selection,
                       const tabs_api::NodeId& tab_to_activate,
                       SetSelectedTabsCallback callback) override;
  void MoveNode(const tabs_api::NodeId& id,
                const tabs_api::Position& position,
                MoveNodeCallback callback) override;

  // tabs_api::mojom::TabStripExperimentalService overrides
  //
  // TabStripExperimentalService is intended for quick prototyping for
  // experimental apis that may not necessarily fit in the standard
  // TabStripService.
  void UpdateTabGroupVisual(const tabs_api::NodeId& id,
                            const tab_groups::TabGroupVisualData& visual_data,
                            UpdateTabGroupVisualCallback) override;

  // tabs_api::observation::TabStripApiBatchedObserver overrides
  void OnTabEvents(
      const std::vector<tabs_api::mojom::TabsEventPtr>& events) override;

  tabs_api::TabStripService* GetTabStripService() const override;

 private:
  std::unique_ptr<tabs_api::TabStripService> tab_strip_service_;

  mojo::ReceiverSet<tabs_api::mojom::TabStripService> clients_;
  mojo::ReceiverSet<tabs_api::mojom::TabStripExperimentService>
      experiment_clients_;
  mojo::AssociatedRemoteSet<tabs_api::mojom::TabsObserver> observers_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_MOJO_HANDLER_H_
