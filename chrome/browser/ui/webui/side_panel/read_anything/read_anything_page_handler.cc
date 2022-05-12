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
  coordinator_->AddObserver(this);
  model_ = coordinator_->GetModel();
  model_->AddObserver(this);
  delegate_ = static_cast<ReadAnythingPageHandler::Delegate*>(
      coordinator_->GetController());
}

ReadAnythingPageHandler::~ReadAnythingPageHandler() {
  // If |this| is destroyed before the |ReadAnythingCoordinator|, then remove
  // |this| from the observer lists. In the cases where the coordinator is
  // destroyed first, these will have been destroyed before this call.
  if (model_)
    model_->RemoveObserver(this);

  if (coordinator_)
    coordinator_->RemoveObserver(this);
}

void ReadAnythingPageHandler::OnUIReady() {
  if (delegate_)
    delegate_->OnUIReady();
}

void ReadAnythingPageHandler::OnCoordinatorDestroyed() {
  coordinator_ = nullptr;
  model_ = nullptr;
  delegate_ = nullptr;
}

void ReadAnythingPageHandler::OnContentUpdated(
    const std::vector<ContentNodePtr>& content_nodes) {
  // Make a copy of |content_nodes|, which is stored in the model, before moving
  // across IPC to the WebUI.
  std::vector<ContentNodePtr> content_nodes_copy;
  for (auto it = content_nodes.begin(); it != content_nodes.end(); ++it)
    content_nodes_copy.push_back(it->Clone());
  page_->ShowContent(std::move(content_nodes_copy));
}

void ReadAnythingPageHandler::OnFontNameUpdated(
    const std::string& new_font_name) {
  page_->OnFontNameChange(new_font_name);
}
