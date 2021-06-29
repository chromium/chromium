// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/side_panel/bookmarks_page_handler.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/read_later/read_later_ui.h"

BookmarksPageHandler::BookmarksPageHandler(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

BookmarksPageHandler::~BookmarksPageHandler() = default;

void BookmarksPageHandler::OpenBookmark(const GURL& url) {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  browser->OpenURL(params);
}
