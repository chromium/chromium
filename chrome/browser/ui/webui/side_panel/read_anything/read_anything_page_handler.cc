// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_page_handler.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"

ReadAnythingPageHandler::ReadAnythingPageHandler(
    mojo::PendingRemote<read_anything::mojom::Page> page,
    mojo::PendingReceiver<read_anything::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {
  // Register |this| as a |ReadAnythingModel::Observer| with the coordinator
  // for the component. This will allow the IPC to update the front-end web ui.

  browser_ = chrome::FindLastActive();
  if (!browser_)
    return;
  ReadAnythingCoordinator::FromBrowser(browser_)->AddModelObserver(this);
  delegate_ =
      ReadAnythingCoordinator::FromBrowser(browser_)->GetPageHandlerDelegate();
}

ReadAnythingPageHandler::~ReadAnythingPageHandler() {
  // Remove |this| from the observer list of |ReadAnythingModel|.
  if (browser_) {
    ReadAnythingCoordinator* coordinator =
        ReadAnythingCoordinator::FromBrowser(browser_);

    // `Browser` is guaranteed to live as long as the ReadAnythingCoordinator
    // since it is BrowserUserData (due to how UserData works - Browser owns
    // this UserData).
    DCHECK(coordinator);
    coordinator->RemoveModelObserver(this);
  }
}

void ReadAnythingPageHandler::ShowUI() {
  delegate_->OnUIShown();
}

void ReadAnythingPageHandler::OnContentUpdated(
    std::vector<std::string> content) {
  page_->OnEssentialContent(content);
}

void ReadAnythingPageHandler::OnFontNameUpdated(
    const std::string& new_font_name) {
  page_->OnFontNameChange(new_font_name);
}
