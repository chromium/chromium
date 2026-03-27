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
  if (group.has_value() || pinned || !index.has_value()) {
    // TODO(crbug.com/494284032): to implement
    NOTREACHED() << "not implemented yet";
  }

  auto* result = model_->OpenTab(url, index.value());
  CHECK(result);
  return result->GetHandle();
}

}  //  namespace tabs_api
