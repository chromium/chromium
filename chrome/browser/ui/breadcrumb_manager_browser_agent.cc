// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/breadcrumb_manager_browser_agent.h"

#include <optional>

#include "base/check_deref.h"
#include "chrome/browser/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "chrome/browser/breadcrumbs/breadcrumb_manager_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/tabs/public/tab_interface.h"

BreadcrumbManagerBrowserAgent::BreadcrumbManagerBrowserAgent(
    TabStripModel* tab_strip_model,
    content::BrowserContext* browser_context)
    : breadcrumb_manager_(CHECK_DEREF(
          BreadcrumbManagerKeyedServiceFactory::GetForBrowserContext(
              browser_context))) {
  tab_strip_model->AddObserver(this);
}

BreadcrumbManagerBrowserAgent::~BreadcrumbManagerBrowserAgent() = default;

int BreadcrumbManagerBrowserAgent::GetTabId(
    const tabs::TabInterface* const tab) const {
  CHECK(tab);
  if (const BreadcrumbManagerTabHelper* const tab_helper =
          BreadcrumbManagerTabHelper::From(tab)) {
    return tab_helper->GetUniqueId();
  }
  auto it = removed_tab_ids_.find(tab->GetHandle().raw_value());
  CHECK(it != removed_tab_ids_.end());
  return it->second;
}

void BreadcrumbManagerBrowserAgent::PlatformLogEvent(const std::string& event) {
  breadcrumb_manager_->AddEvent(event);
}

void BreadcrumbManagerBrowserAgent::OnTabWillBeRemoved(tabs::TabInterface* tab,
                                                       int index) {
  CHECK(tab);
  if (const BreadcrumbManagerTabHelper* const tab_helper =
          BreadcrumbManagerTabHelper::From(tab)) {
    removed_tab_ids_[tab->GetHandle().raw_value()] = tab_helper->GetUniqueId();
  }
}

void BreadcrumbManagerBrowserAgent::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kInserted: {
      const auto& inserted_tabs = change.GetInsert()->contents;
      const int num_inserted_tabs = inserted_tabs.size();
      if (num_inserted_tabs == 1) {
        LogTabInsertedAt(GetTabId(inserted_tabs[0].tab), inserted_tabs[0].index,
                         selection.active_tab_changed());
      } else {
        LogTabsInserted(num_inserted_tabs);
      }
      return;
    }
    case TabStripModelChange::kRemoved: {
      const auto& closed_tabs = change.GetRemove()->contents;
      const int num_closed_tabs = closed_tabs.size();
      if (num_closed_tabs == 1) {
        LogTabClosedAt(GetTabId(closed_tabs[0].tab), closed_tabs[0].index);
      } else {
        LogTabsClosed(num_closed_tabs);
      }
      removed_tab_ids_.clear();
      return;
    }
    case TabStripModelChange::kMoved: {
      const auto* const move = change.GetMove();
      LogTabMoved(GetTabId(move->tab), move->from_index, move->to_index);
      return;
    }
    case TabStripModelChange::kReplaced: {
      const auto* const replace = change.GetReplace();
      const BreadcrumbManagerTabHelper* const tab_helper =
          BreadcrumbManagerTabHelper::From(replace->tab);
      CHECK(tab_helper);
      CHECK(tab_helper->GetPreviousUniqueId().has_value());
      LogTabReplaced(*tab_helper->GetPreviousUniqueId(),
                     tab_helper->GetUniqueId(), replace->index);
      return;
    }
    case TabStripModelChange::kSelectionOnly: {
      if (selection.active_tab_changed()) {
        std::optional<int> old_tab_id =
            selection.old_tab ? std::make_optional(GetTabId(selection.old_tab))
                              : std::nullopt;
        std::optional<int> new_tab_id =
            selection.new_tab ? std::make_optional(GetTabId(selection.new_tab))
                              : std::nullopt;
        LogActiveTabChanged(old_tab_id, new_tab_id,
                            selection.new_model.active());
      }
      return;
    }
  }
}
