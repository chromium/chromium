// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"

#include "base/types/expected.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "url/gurl.h"

TabStripServiceImpl::TabStripServiceImpl(BrowserWindowInterface* browser,
                                         TabStripModel* tab_strip_model)
    : browser_(browser), model_(tab_strip_model) {
  model_->AddObserver(this);
}

TabStripServiceImpl::~TabStripServiceImpl() {
  if (model_) {
    model_->RemoveObserver(this);
  }

  // Clear all observers
  // TODO (crbug.com/412955607): Implement a removal mechanism similar to
  // TabStripModelObserver where on shutdown of the TabStripService, it notifies
  // to all clients that service is shutting down.
  observers_.Clear();
}

void TabStripServiceImpl::CreateTabAt(tabs_api::mojom::PositionPtr pos,
                                      const std::optional<GURL>& url,
                                      CreateTabAtCallback callback) {
  GURL target_url;
  if (url.has_value()) {
    target_url = url.value();
  }
  int index = -1;
  if (pos) {
    index = pos->index;
  }

  content::WebContents* content = AddTabAt(target_url, index);
  if (!content) {
    // Missing content can happen for a number of reasons. i.e. If the profile
    // is shutting down or if navigation requests are blocked due to some
    // internal state. This is usually because the browser is not in the
    // required state to perform the action.
    std::move(callback).Run(base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kFailedPrecondition,
        "Failed to create WebContents")));
  } else {
    std::move(callback).Run(base::ok(true));
  }
}

content::WebContents* TabStripServiceImpl::AddTabAt(const GURL& url,
                                                    int index) {
  // TODO (crbug.com/411134070) chrome::AddAndReturnTabAt does not support
  // BrowserWindowInterface. Navigation should handle BrowserWindowInterface
  // instead of Browser.
  return chrome::AddAndReturnTabAt(browser_->GetBrowserForMigrationOnly(), url,
                                   index, true);
}

void TabStripServiceImpl::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kInserted:
      OnTabStripModelChangeAdded(*change.GetInsert());
      break;
    case TabStripModelChange::kRemoved:
    case TabStripModelChange::kReplaced:
    case TabStripModelChange::kMoved:
    case TabStripModelChange::kSelectionOnly:
      break;
  }
}

void TabStripServiceImpl::OnTabStripModelChangeAdded(
    const TabStripModelChange::Insert& insert_change) {
  if (insert_change.contents.size() == 0) {
    return;
  }

  std::vector<tabs_api::mojom::PositionPtr> positions;
  for (const auto& content : insert_change.contents) {
    auto pos = tabs_api::mojom::Position::New();
    pos->index = content.index;
    positions.emplace_back(std::move(pos));
  }

  for (auto& observer : observers_) {
    observer.OnTabsCreated(std::move(positions));
  }
}

void TabStripServiceImpl::AddObserver(tabs_api::mojom::TabsObserver* observer) {
  observers_.AddObserver(observer);
}

void TabStripServiceImpl::RemoveObserver(
    tabs_api::mojom::TabsObserver* observer) {
  observers_.RemoveObserver(observer);
}
