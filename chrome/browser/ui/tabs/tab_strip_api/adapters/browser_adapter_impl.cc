// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter_impl.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter_impl.h"
#include "components/sessions/core/session_id.h"

namespace tabs_api {

// Magic number to signal new tab should be appended.
constexpr int kAppendNewTab = -1;

std::vector<std::unique_ptr<TabStripModelAdapter>>
BrowserAdapterImpl::CreateAllTabStripModelAdaptersForProfile() {
  std::vector<std::unique_ptr<TabStripModelAdapter>> results;
  for (BrowserWindowInterface* window : GetAllBrowserWindowInterfaces()) {
    if (window->GetProfile() == browser_->GetProfile()) {
      TabStripModel* tab_strip_model = window->GetTabStripModel();
      if (tab_strip_model) {
        results.push_back(std::make_unique<TabStripModelAdapterImpl>(
            tab_strip_model,
            base::NumberToString(window->GetSessionID().id())));
      }
    }
  }
  return results;
}

tabs::TabHandle BrowserAdapterImpl::AddTabAt(
    const GURL& url,
    std::optional<int> index,
    std::optional<tab_groups::TabGroupId> group,
    bool pinned) {
  auto* contents = chrome::AddAndReturnTabAt(
      browser_->GetBrowserForMigrationOnly(), url,
      index.value_or(kAppendNewTab), true, group, pinned);
  return contents ? tabs::TabInterface::GetFromContents(contents)->GetHandle()
                  : tabs::TabHandle::Null();
}

}  // namespace tabs_api
