// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_mojo_handler.h"

#include "base/types/expected.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/event_broadcaster.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

TabStripServiceMojoHandler::TabStripServiceMojoHandler(
    BrowserWindowInterface* browser,
    TabStripModel* tab_strip_model)
    : TabStripServiceMojoHandler(
          std::make_unique<tabs_api::TabStripServiceImpl>(browser,
                                                          tab_strip_model),
          std::make_unique<tabs_api::TabStripModelAdapterImpl>(
              tab_strip_model)) {}

TabStripServiceMojoHandler::TabStripServiceMojoHandler(
    std::unique_ptr<tabs_api::TabStripService> service,
    std::unique_ptr<tabs_api::TabStripModelAdapter> tab_strip_model_adapter)
    : tab_strip_service_(std::move(service)) {
  tab_strip_service_->AddObserver(this);
}

TabStripServiceMojoHandler::~TabStripServiceMojoHandler() {
  tab_strip_service_->RemoveObserver(this);

  // Clear all observers
  // TODO (crbug.com/412955607): Implement a removal mechanism similar to
  // TabStripModelObserver where on shutdown of the TabStripService, it notifies
  // to all clients that service is shutting down.
  observers_.Clear();
}

void TabStripServiceMojoHandler::GetTabs(GetTabsCallback callback) {
  auto snapshot = tabs_api::mojom::TabsSnapshot::New();
  auto result = tab_strip_service_->GetTabs();
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(result.error())));
  }
  snapshot->tab_strip = std::move(result.value());

  mojo::AssociatedRemote<tabs_api::mojom::TabsObserver> stream;
  auto pending_receiver = stream.BindNewEndpointAndPassReceiver();
  observers_.Add(std::move(stream));
  snapshot->stream = std::move(pending_receiver);

  std::move(callback).Run(std::move(snapshot));
}

void TabStripServiceMojoHandler::GetTab(const tabs_api::NodeId& tab_mojom_id,
                                        GetTabCallback callback) {
  std::move(callback).Run(tab_strip_service_->GetTab(tab_mojom_id));
}

void TabStripServiceMojoHandler::CreateTabAt(
    const std::optional<tabs_api::Position>& pos,
    const std::optional<GURL>& url,
    CreateTabAtCallback callback) {
  std::move(callback).Run(tab_strip_service_->CreateTabAt(pos, url));
}

void TabStripServiceMojoHandler::CloseTabs(
    const std::vector<tabs_api::NodeId>& ids,
    CloseTabsCallback callback) {
  std::move(callback).Run(tab_strip_service_->CloseTabs(ids));
}

void TabStripServiceMojoHandler::ActivateTab(const tabs_api::NodeId& id,
                                             ActivateTabCallback callback) {
  std::move(callback).Run(tab_strip_service_->ActivateTab(id));
}

void TabStripServiceMojoHandler::SetSelectedTabs(
    const std::vector<tabs_api::NodeId>& selection,
    const tabs_api::NodeId& tab_to_activate,
    SetSelectedTabsCallback callback) {
  std::move(callback).Run(
      tab_strip_service_->SetSelectedTabs(selection, tab_to_activate));
}

void TabStripServiceMojoHandler::MoveNode(const tabs_api::NodeId& id,
                                          const tabs_api::Position& position,
                                          MoveNodeCallback callback) {
  std::move(callback).Run(tab_strip_service_->MoveNode(id, position));
}

void TabStripServiceMojoHandler::UpdateTabGroupVisual(
    const tabs_api::NodeId& id,
    const tab_groups::TabGroupVisualData& visual_data,
    UpdateTabGroupVisualCallback callback) {
  std::move(callback).Run(
      tab_strip_service_->UpdateTabGroupVisual(id, visual_data));
}

void TabStripServiceMojoHandler::OnTabEvents(
    const std::vector<tabs_api::mojom::TabsEventPtr>& events) {
  for (auto& observer : observers_) {
    std::vector<tabs_api::mojom::TabsEventPtr> copy;
    for (auto& event : events) {
      copy.push_back(event.Clone());
    }
    observer->OnTabEvents(std::move(copy));
  }
}

tabs_api::TabStripService* TabStripServiceMojoHandler::GetTabStripService()
    const {
  return tab_strip_service_.get();
}

void TabStripServiceMojoHandler::Accept(
    mojo::PendingReceiver<tabs_api::mojom::TabStripService> client) {
  clients_.Add(this, std::move(client));
}

void TabStripServiceMojoHandler::AcceptExperimental(
    mojo::PendingReceiver<tabs_api::mojom::TabStripExperimentService> client) {
  experiment_clients_.Add(this, std::move(client));
}
