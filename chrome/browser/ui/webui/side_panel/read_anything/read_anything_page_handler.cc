// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_page_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"
#include "ui/accessibility/ax_tree_update.h"

using read_anything::mojom::Page;
using read_anything::mojom::PageHandler;
using read_anything::mojom::ReadAnythingThemePtr;

ReadAnythingPageHandler::ReadAnythingPageHandler(
    mojo::PendingRemote<Page> page,
    mojo::PendingReceiver<PageHandler> receiver)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {
  // Register |this| as a |ReadAnythingModel::Observer| with the coordinator
  // for the component. This will allow the IPC to update the front-end web ui.

  browser_ = chrome::FindLastActive();
  if (!browser_)
    return;

  coordinator_ = ReadAnythingCoordinator::FromBrowser(browser_);
  if (coordinator_) {
    coordinator_->AddObserver(this);
    coordinator_->AddModelObserver(this);
  }

  delegate_ = static_cast<ReadAnythingPageHandler::Delegate*>(
      coordinator_->GetController());
  if (delegate_)
    delegate_->OnUIReady();
}

ReadAnythingPageHandler::~ReadAnythingPageHandler() {
  delegate_ = static_cast<ReadAnythingPageHandler::Delegate*>(
      coordinator_->GetController());
  if (delegate_)
    delegate_->OnUIDestroyed();

  // If |this| is destroyed before the |ReadAnythingCoordinator|, then remove
  // |this| from the observer lists. In the cases where the coordinator is
  // destroyed first, these will have been destroyed before this call.
  if (coordinator_) {
    coordinator_->RemoveObserver(this);
    coordinator_->RemoveModelObserver(this);
  }
}

void ReadAnythingPageHandler::OnCoordinatorDestroyed() {
  coordinator_ = nullptr;
  delegate_ = nullptr;
}

void ReadAnythingPageHandler::OnAXTreeDistilled(
    const ui::AXTreeUpdate& snapshot,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  page_->OnAXTreeDistilled(snapshot, content_node_ids);
}

void ReadAnythingPageHandler::OnReadAnythingThemeChanged(
    ReadAnythingThemePtr new_theme_ptr) {
  page_->OnThemeChanged(std::move(new_theme_ptr));
}
