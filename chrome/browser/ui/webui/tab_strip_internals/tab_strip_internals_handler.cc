// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_handler.h"

#include <utility>

TabStripInternalsPageHandler::TabStripInternalsPageHandler(
    mojo::PendingReceiver<tab_strip_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<tab_strip_internals::mojom::Page> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {}

TabStripInternalsPageHandler::~TabStripInternalsPageHandler() = default;

void TabStripInternalsPageHandler::GetTabStripData(
    GetTabStripDataCallback callback) {
  auto data = tab_strip_internals::mojom::Container::New();

  // TODO (crbug.com/427204855): Follow-up with actual implementation to
  // populate tabstrip data.

  // TODO (crbug.com/427204855): Inherit from TabStripModelObserver,
  // BrowserListObserver, and TabRestoreServiceObserver and implement required
  // methods to listen to and broadcast live-updates to the webui.
  std::move(callback).Run(std::move(data));
}

// TODO (crbug.com/427204855): Invoke this method from TabStrip observer
// methods.
void TabStripInternalsPageHandler::NotifyTabStripUpdated(
    tab_strip_internals::mojom::ContainerPtr data) {
  if (page_) {
    page_->OnTabStripUpdated(std::move(data));
  }
}
