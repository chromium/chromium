// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/breadcrumb_manager_browser_agent.h"

#include <optional>

#include "chrome/browser/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "chrome/browser/breadcrumbs/breadcrumb_manager_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"

namespace {

int GetTabId(const content::WebContents* const web_contents) {
  CHECK(web_contents);
  const BreadcrumbManagerTabHelper* const tab_helper =
      BreadcrumbManagerTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);
  return tab_helper->GetUniqueId();
}

}  // namespace

BreadcrumbManagerBrowserAgent::BreadcrumbManagerBrowserAgent(Browser* browser)
    : browser_(browser) {
  browser_->tab_strip_model()->AddObserver(this);
}

BreadcrumbManagerBrowserAgent::~BreadcrumbManagerBrowserAgent() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

void BreadcrumbManagerBrowserAgent::PlatformLogEvent(const std::string& event) {
  BreadcrumbManagerKeyedServiceFactory::GetForBrowserContext(
      browser_->profile())
      ->AddEvent(event);
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
        LogTabInsertedAt(GetTabId(inserted_tabs[0].contents),
                         inserted_tabs[0].index,
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
        LogTabClosedAt(GetTabId(closed_tabs[0].contents), closed_tabs[0].index);
      } else {
        LogTabsClosed(num_closed_tabs);
      }
      return;
    }
    case TabStripModelChange::kMoved: {
      const auto* const move = change.GetMove();
      LogTabMoved(GetTabId(move->contents), move->from_index, move->to_index);
      return;
    }
    case TabStripModelChange::kReplaced: {
      const auto* const replace = change.GetReplace();
      LogTabReplaced(GetTabId(replace->old_contents),
                     GetTabId(replace->new_contents), replace->index);
      return;
    }
    case TabStripModelChange::kSelectionOnly: {
      if (selection.active_tab_changed()) {
        std::optional<int> old_tab_id =
            selection.old_contents
                ? std::make_optional(GetTabId(selection.old_contents))
                : std::nullopt;
        std::optional<int> new_tab_id =
            selection.new_contents
                ? std::make_optional(GetTabId(selection.new_contents))
                : std::nullopt;
        LogActiveTabChanged(old_tab_id, new_tab_id,
                            selection.new_model.active());
      }
      return;
    }
  }
}
