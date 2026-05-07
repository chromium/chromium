// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_browser_adapter_impl.h"

#include "base/check_deref.h"
#include "base/notreached.h"

namespace tabs_api {

AndroidBrowserAdapterImpl::AndroidBrowserAdapterImpl(TabModel* model)
    : model_(CHECK_DEREF(model)) {}

std::vector<std::unique_ptr<TabStripModelAdapter>>
AndroidBrowserAdapterImpl::CreateAllTabStripModelAdaptersForProfile() {
  NOTREACHED() << "not implemented";
}

tabs::TabHandle AndroidBrowserAdapterImpl::AddTabAt(
    const GURL& url,
    std::optional<int> index,
    std::optional<tab_groups::TabGroupId> group,
    bool pinned) {
  int target_index = index.value_or(model_->GetTabCount());

  auto* result = model_->OpenTab(url, target_index);
  CHECK(result);
  tabs::TabHandle handle = result->GetHandle();

  if (pinned) {
    model_->PinTab(handle);
  }

  if (group.has_value()) {
    model_->AddTabsToGroup(group.value(), {handle});
  }

  return handle;
}

}  //  namespace tabs_api
