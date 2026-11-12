// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/scoped_test_tab_strip.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/browser_apis/tab_strip/events/event.h"
#include "components/browser_apis/tab_strip/tab_strip_api_events.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_service_view.h"
#include "components/browser_apis/tab_strip/testing/injector.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/browser_apis/tab_strip/types/path.h"
#include "components/browser_apis/tab_strip/types/position.h"
#include "components/browser_apis/tab_strip/types/tab.h"
#include "components/tabs/public/tab_network_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace tabs_api::testing {

namespace {

Tab BuildTabFromInterface(tabs::TabInterface* tab_interface, bool is_active) {
  Tab tab;
  tab.id = NodeId::FromTabHandle(tab_interface->GetHandle());
  content::WebContents* contents = tab_interface->GetContents();
  tab.title = contents ? contents->GetTitle() : tab_interface->GetTitle();
  tab.url = contents ? contents->GetVisibleURL() : tab_interface->GetURL();
  tab.network_state = contents ? tabs::TabNetworkStateForWebContents(contents)
                               : tabs::TabNetworkState::kNone;
  tab.is_active = is_active;
  tab.last_active =
      contents ? std::max(contents->GetLastActiveTimeTicks(),
                          contents->GetLastInteractionTimeTicks())
               : (base::TimeTicks::Now() -
                  (base::Time::Now() - tab_interface->GetLastActiveTime()));
  return tab;
}

}  // namespace

class ScopedTestTabStrip::TabContentsObserver
    : public content::WebContentsObserver {
 public:
  TabContentsObserver(ScopedTestTabStrip& owner,
                      tabs::TabHandle handle,
                      content::WebContents* contents)
      : content::WebContentsObserver(contents),
        owner_(owner),
        handle_(handle) {}

  tabs::TabHandle handle() const { return handle_; }

  void DidStartLoading() override { NotifyOwner(); }
  void DidStopLoading() override { NotifyOwner(); }
  void DidFinishNavigation(content::NavigationHandle*) override {
    NotifyOwner();
  }
  void TitleWasSet(content::NavigationEntry*) override { NotifyOwner(); }

 private:
  void NotifyOwner() {
    if (tabs::TabInterface* tab = handle_.Get()) {
      owner_->UpdateTab(tab);
    }
  }

  raw_ref<ScopedTestTabStrip> owner_;
  tabs::TabHandle handle_;
};

ScopedTestTabStrip::ScopedTestTabStrip(ui::UnownedUserDataHost& host) {
  auto injector = std::make_unique<Injector>(toy_tab_strip_);
  event_bridge_ = &injector->event_bridge();
  feature_ =
      std::make_unique<TabStripServiceFeature>(std::move(injector), host);
}

ScopedTestTabStrip::~ScopedTestTabStrip() {
  tab_observers_.clear();
  event_bridge_ = nullptr;
  feature_.reset();
}

void ScopedTestTabStrip::AddTab(tabs::TabInterface* tab, bool active) {
  CHECK(tab);
  content::WebContents* contents = tab->GetContents();
  const size_t index = toy_tab_strip_.GetTabs().size();
  const Tab built = BuildTabFromInterface(tab, active);
  toy_tab_strip_.AddTab(ToyTab{
      .tab_handle = tab->GetHandle(),
      .gurl = built.url,
      .active = active,
      .title = built.title,
      .last_active = built.last_active,
  });
  if (active) {
    toy_tab_strip_.ActivateTab(tab->GetHandle());
  }
  if (contents) {
    tab_observers_.push_back(std::make_unique<TabContentsObserver>(
        *this, tab->GetHandle(), contents));
  }
  NotifyTabCreated(built, index);
}

tabs::TabHandle ScopedTestTabStrip::AddTab(const GURL& url,
                                           std::u16string title,
                                           bool active) {
  const size_t index = toy_tab_strip_.GetTabs().size();
  const tabs::TabHandle handle = toy_tab_strip_.AddTabAt(url, std::nullopt);
  toy_tab_strip_.CloseTab(index);
  const base::TimeTicks now = base::TimeTicks::Now();
  toy_tab_strip_.AddTab(ToyTab{
      .tab_handle = handle,
      .gurl = url,
      .active = active,
      .title = title,
      .last_active = now,
  });
  if (active) {
    toy_tab_strip_.ActivateTab(handle);
  }

  Tab tab;
  tab.id = NodeId::FromTabHandle(handle);
  tab.url = url;
  tab.title = std::move(title);
  tab.is_active = active;
  tab.last_active = now;
  NotifyTabCreated(tab, index);
  return handle;
}

void ScopedTestTabStrip::SetActiveTab(tabs::TabInterface* tab) {
  CHECK(tab);
  SetActiveTab(tab->GetHandle());
}

void ScopedTestTabStrip::SetActiveTab(tabs::TabHandle handle) {
  const TabStripServiceView* view = feature_->GetTabStripServiceView();
  const Tab* prev_active = view->GetActiveTab();
  const std::optional<NodeId> old_active_id =
      prev_active ? std::optional<NodeId>(prev_active->id) : std::nullopt;
  const NodeId new_active_id = NodeId::FromTabHandle(handle);

  toy_tab_strip_.ActivateTab(handle);

  std::vector<events::Event> events;
  if (old_active_id && *old_active_id != new_active_id) {
    if (const Tab* old_snapshot = view->GetTab(*old_active_id)) {
      auto old_change = mojom::TabChange::New();
      if (std::optional<tabs::TabHandle> old_handle =
              old_snapshot->ToTabHandle();
          old_handle && old_handle->Get()) {
        old_change->data =
            BuildTabFromInterface(old_handle->Get(), /*is_active=*/false);
      } else {
        old_change->data = *old_snapshot;
        old_change->data.is_active = false;
      }
      old_change->mask = mojom::TabFieldMask::New();
      old_change->mask->is_active = true;
      events.emplace_back(
          mojom::OnDataChangedEvent::NewTab(std::move(old_change)));
    }
  }

  auto new_change = mojom::TabChange::New();
  if (tabs::TabInterface* tab_interface = handle.Get()) {
    new_change->data = BuildTabFromInterface(tab_interface, /*is_active=*/true);
  } else if (const Tab* existing = view->GetTab(new_active_id)) {
    new_change->data = *existing;
    new_change->data.is_active = true;
  } else {
    new_change->data.id = new_active_id;
    new_change->data.is_active = true;
  }
  new_change->mask = mojom::TabFieldMask::New();
  new_change->mask->is_active = true;
  events.emplace_back(mojom::OnDataChangedEvent::NewTab(std::move(new_change)));
  event_bridge_->NotifyEvents(events);
}

void ScopedTestTabStrip::UpdateTab(tabs::TabInterface* tab) {
  CHECK(tab);
  const Tab* prev_active = feature_->GetTabStripServiceView()->GetActiveTab();
  const bool is_active =
      prev_active && prev_active->ToTabHandle() == tab->GetHandle();
  auto change = mojom::TabChange::New();
  change->data = BuildTabFromInterface(tab, is_active);
  change->mask = mojom::TabFieldMask::New();
  std::vector<events::Event> events;
  events.emplace_back(mojom::OnDataChangedEvent::NewTab(std::move(change)));
  event_bridge_->NotifyEvents(events);
}

void ScopedTestTabStrip::CloseTab(tabs::TabHandle handle) {
  std::erase_if(tab_observers_,
                [handle](const std::unique_ptr<TabContentsObserver>& observer) {
                  return observer->handle() == handle;
                });
  if (std::optional<int> index = toy_tab_strip_.GetIndexForHandle(handle)) {
    toy_tab_strip_.CloseTab(static_cast<size_t>(*index));
  }
  auto closed_event = mojom::OnNodesClosedEvent::New();
  closed_event->node_ids.push_back(NodeId::FromTabHandle(handle));
  std::vector<events::Event> events;
  events.emplace_back(std::move(closed_event));
  event_bridge_->NotifyEvents(events);
}

void ScopedTestTabStrip::CloseTab(tabs::TabInterface* tab) {
  CHECK(tab);
  CloseTab(tab->GetHandle());
}

void ScopedTestTabStrip::NotifyTabCreated(const Tab& tab, size_t index) {
  auto created_event = mojom::OnTabsCreatedEvent::New();
  auto created = mojom::TabCreatedContainer::New();
  created->tab = tab;
  created->position =
      Position(index, Path({
                          NodeId(NodeId::Type::kWindow, "1"),
                          NodeId(NodeId::Type::kCollection, "0"),
                      }));
  created_event->tabs.push_back(std::move(created));
  std::vector<events::Event> events;
  events.emplace_back(std::move(created_event));
  event_bridge_->NotifyEvents(events);
}

}  // namespace tabs_api::testing
