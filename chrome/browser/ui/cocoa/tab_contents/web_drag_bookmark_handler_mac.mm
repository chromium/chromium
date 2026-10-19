// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/web_drag_bookmark_handler_mac.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"

using content::WebContents;

WebDragBookmarkHandlerMac::WebDragBookmarkHandlerMac() = default;

WebDragBookmarkHandlerMac::~WebDragBookmarkHandlerMac() = default;

void WebDragBookmarkHandlerMac::DragInitialize(WebContents* contents) {
  web_contents_ = contents;
  if (!bookmark_drag_) {
    bookmark_drag_ =
        extensions::BookmarkManagerPrivateDragEventRouter::FromWebContents(
            contents);
  }

  // This operation is synchronous on Mac.
  bookmarks::BookmarkNodeData::ReadFromClipboard(
      ui::ClipboardBuffer::kDrag,
      base::BindOnce(
          [](bookmarks::BookmarkNodeData* bookmark_drag_data,
             std::unique_ptr<bookmarks::BookmarkNodeData> data) {
            if (data) {
              *bookmark_drag_data = std::move(*data);
            }
          },
          &bookmark_drag_data_));
}

void WebDragBookmarkHandlerMac::OnDragOver() {
  if (bookmark_drag_) {
    bookmark_drag_->OnDragOver(bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerMac::OnDragEnter() {
  if (bookmark_drag_) {
    bookmark_drag_->OnDragEnter(bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerMac::OnDrop() {
  // This is non-null if the web_contents_ is showing an ExtensionUrlOverrides
  // with support for (at the moment experimental) drag and drop extensions.
  if (bookmark_drag_) {
    bookmark_drag_->OnDrop(bookmark_drag_data_);

    // Focus the target browser.
    BrowserWindowInterface* browser =
        GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
            web_contents_);
    if (browser) {
      browser->GetWindow()->Show();
    }
  }
}

void WebDragBookmarkHandlerMac::OnDragLeave() {
  if (bookmark_drag_) {
    bookmark_drag_->OnDragLeave(bookmark_drag_data_);
  }
}
