// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_IMPL_H_

#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/context_menu_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/translation_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/events/event.h"
#include "chrome/browser/ui/tabs/tab_strip_api/observation/tab_strip_api_batched_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_experiment_api.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

namespace events {
class TabStripEventRecorder;
}  // namespace events

class PlatformAdaptersProvider;
class ExperimentalPlatformAdaptersProvider;

// To prevent re-entrancy, we use a session recorder to queue up all events
// received during a subroutine. Once subroutine completes, we notify all
// clients of the observed events before returning the result of the method
// invocation. This behaviour is important for the mojo layer. See the tab
// strip api mojo handler for more details.
class TabStripServiceImpl
    : public TabStripService,
      public tabs_api::observation::TabStripApiBatchedObserver {
 public:
  TabStripServiceImpl(
      std::unique_ptr<PlatformAdaptersProvider> adapters_provider,
      std::unique_ptr<ExperimentalPlatformAdaptersProvider>
          experimental_adapters_provider);
  TabStripServiceImpl(const TabStripServiceImpl&) = delete;
  TabStripServiceImpl operator=(const TabStripServiceImpl&&) = delete;
  ~TabStripServiceImpl() override;

  TabStripService::GetTabsResult GetTabsWithoutObservation() override;
  void Accept(
      mojo::PendingReceiver<tabs_api::mojom::TabStripService> client) override;
  void AcceptExperimental(
      mojo::PendingReceiver<tabs_api::mojom::TabStripExperimentService> client)
      override;

  // TabStripServiceDirectReturnStub:
  mojom::TabStripService::GetTabsResult GetTabs() override;
  mojom::TabStripService::GetTabResult GetTab(
      const tabs_api::NodeId& id) override;
  mojom::TabStripService::CreateTabAtResult CreateTabAt(
      const std::optional<tabs_api::Position>& pos,
      const std::optional<GURL>& url) override;
  mojom::TabStripService::CloseNodesResult CloseNodes(
      const std::vector<tabs_api::NodeId>& ids) override;
  mojom::TabStripService::ActivateTabResult ActivateTab(
      const tabs_api::NodeId& id) override;
  mojom::TabStripService::SetSelectedTabsResult SetSelectedTabs(
      const std::vector<tabs_api::NodeId>& selection,
      const tabs_api::NodeId& tab_to_activate) override;
  mojom::TabStripService::MoveNodeResult MoveNode(
      const tabs_api::NodeId& id,
      const tabs_api::Position& position) override;
  mojom::TabStripService::UpdateResult Update(
      mojom::DataPtr data,
      const std::optional<std::vector<std::string>>& update_mask) override;

  // tabs_api::mojom::TabStripExperimentalService overrides
  //
  // TabStripExperimentalService is intended for quick prototyping for
  // experimental apis that may not necessarily fit in the standard
  // TabStripService.
  mojom::TabStripExperimentService::ReplaceTabInSplitResult ReplaceTabInSplit(
      const tabs_api::NodeId& tab_to_replace,
      const tabs_api::NodeId& tab_to_insert) override;

  mojom::TabStripExperimentService::ShowTabContextMenuResult ShowTabContextMenu(
      const tabs_api::NodeId& tab_id,
      const gfx::Point& location) override;
  mojom::TabStripExperimentService::GetAllTabsForProfileResult
  GetAllTabsForProfile() override;

  void AddObserver(observation::TabStripApiBatchedObserver* observer) override;
  void RemoveObserver(
      observation::TabStripApiBatchedObserver* observer) override;

  // tabs_api::observation::TabStripApiBatchedObserver overrides
  void OnTabEvents(
      const std::vector<tabs_api::mojom::TabsEventPtr>& events) override;

  // Used internally by the tab strip service to control API invocation rules.
  // A session represents an ongoing API invocation.
  class Session {
   public:
    virtual ~Session() = default;
  };

  // Used to create sessions.
  class SessionController {
   public:
    virtual ~SessionController() = default;
    virtual std::unique_ptr<Session> CreateSession() = 0;
  };

 private:
  mojom::TabStripServiceBridge bridge_{this};
  mojom::TabStripExperimentServiceBridge experimental_bridge_{this};

  TabStripModelAdapter& tab_strip_model_adapter();
  TranslationAdapter& translation_adapter();
  BrowserAdapter& browser_adapter();
  ContextMenuAdapter* context_menu_adapter();

  void BroadcastEvents(
      const std::vector<tabs_api::events::Event>& events) const;

  base::expected<void, mojo_base::mojom::ErrorPtr> CloseCollection(
      const NodeId& id);
  void CloseTabs(const std::vector<tabs::TabHandle>& tab_targets);

  mojom::TabStripService::UpdateResult UpdateTabGroup(
      mojom::TabGroupPtr tab_group,
      const std::optional<std::vector<std::string>>& update_mask);

  std::unique_ptr<PlatformAdaptersProvider> adapters_provider_;
  std::unique_ptr<ExperimentalPlatformAdaptersProvider>
      experimental_adapters_provider_;
  std::unique_ptr<events::TabStripEventRecorder> recorder_;

  std::unique_ptr<SessionController> session_controller_;
  base::ObserverList<observation::TabStripApiBatchedObserver> observers_;

  mojo::ReceiverSet<tabs_api::mojom::TabStripService> mojo_clients_;
  mojo::ReceiverSet<tabs_api::mojom::TabStripExperimentService>
      mojo_experiment_clients_;
  mojo::AssociatedRemoteSet<tabs_api::mojom::TabsObserver> mojo_observers_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_IMPL_H_
