// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_handler.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_util.h"
#include "components/sessions/core/session_id.h"

TabStripInternalsPageHandler::TabStripInternalsPageHandler(
    mojo::PendingReceiver<tab_strip_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<tab_strip_internals::mojom::Page> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {
  observer_ = std::make_unique<TabStripInternalsObserver>(
      base::BindRepeating(&TabStripInternalsPageHandler::NotifyTabStripUpdated,
                          weak_ptr_factory_.GetWeakPtr()));
}

TabStripInternalsPageHandler::~TabStripInternalsPageHandler() = default;

void TabStripInternalsPageHandler::GetTabStripData(
    GetTabStripDataCallback callback) {
  std::move(callback).Run(BuildSnapshot());
}

tab_strip_internals::mojom::ContainerPtr
TabStripInternalsPageHandler::BuildSnapshot() {
  auto data = tab_strip_internals::mojom::Container::New();
  data->tabstrip_tree = tab_strip_internals::mojom::TabStripTree::New();
  // TODO (crbug.com/427204855): Add tab restore and session restore data.

  for (const auto* browser : GetAllBrowserWindowInterfaces()) {
    auto window_node = tab_strip_internals::mojom::WindowNode::New();

    window_node->id = tab_strip_internals::MakeNodeId(
        base::NumberToString(browser->GetSessionID().id()),
        tab_strip_internals::mojom::NodeId::Type::kWindow);

    window_node->tabstrip_model =
        tab_strip_internals::mojom::TabStripModel::New(
            tab_strip_internals::BuildTabCollectionTree(
                browser->GetTabStripModel()));

    window_node->selection_model =
        tab_strip_internals::BuildSelectionModel(browser->GetTabStripModel());

    data->tabstrip_tree->windows.push_back(std::move(window_node));
  }

  // TODO (crbug.com/427204855): Inherit from TabRestoreServiceObserver and
  // implement required methods to listen to and broadcast live-updates to the
  // webui.
  return data;
}

void TabStripInternalsPageHandler::NotifyTabStripUpdated() {
  if (page_) {
    page_->OnTabStripUpdated(BuildSnapshot());
  }
}
