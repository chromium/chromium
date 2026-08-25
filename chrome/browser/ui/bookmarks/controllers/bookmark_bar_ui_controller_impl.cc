// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_controller_impl.h"

#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_client.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

BookmarkBarUIControllerImpl::BookmarkBarUIControllerImpl(
    BrowserWindowInterface* browser)
    : browser_(browser) {}

BookmarkBarUIControllerImpl::~BookmarkBarUIControllerImpl() = default;

void BookmarkBarUIControllerImpl::Bind(BookmarkBarUIClient* client) {
  client_ = client;
}
