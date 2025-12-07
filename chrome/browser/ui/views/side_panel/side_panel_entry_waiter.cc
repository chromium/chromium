// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_entry_waiter.h"

#include <optional>

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"

SidePanelEntryWaiter::SidePanelEntryWaiter() = default;
SidePanelEntryWaiter::~SidePanelEntryWaiter() = default;

void SidePanelEntryWaiter::WaitForEntry(
    SidePanelEntry* entry,
    SidePanelEntryWaiter::PopulateSidePanelCallback callback) {
  CHECK(entry);
  ResetLoadingEntryIfNecessary();
  auto content_view = entry->GetContent();
  SidePanelContentProxy* content_proxy =
      SidePanelUtil::GetSidePanelContentProxy(content_view.get());
  if (content_proxy->IsAvailable() || show_immediately_for_testing_) {
    std::move(callback).Run(entry, std::move(content_view));
  } else {
    entry->CacheView(std::move(content_view));
    loading_entry_ = entry->GetWeakPtr();
    loaded_callback_.Reset(
        base::BindOnce(&SidePanelEntryWaiter::RunLoadedCallback,
                       base::Unretained(this), std::move(callback)));
    content_proxy->SetAvailableCallback(loaded_callback_.callback());
  }
}

void SidePanelEntryWaiter::ResetLoadingEntryIfNecessary() {
  loading_entry_.reset();
  loaded_callback_.Cancel();
}

void SidePanelEntryWaiter::SetNoDelaysForTesting(bool no_delays_for_testing) {
  show_immediately_for_testing_ = no_delays_for_testing;
}

void SidePanelEntryWaiter::RunLoadedCallback(
    PopulateSidePanelCallback callback) {
  // content_proxy is owned by content_view which is owned by SidePanelEntry.
  // If this callback runs then loading_entry_ must be valid.
  if (loading_entry_) {
    SidePanelEntry* entry = loading_entry_.get();
    loading_entry_.reset();
    std::move(callback).Run(entry, std::nullopt);
  }
}
