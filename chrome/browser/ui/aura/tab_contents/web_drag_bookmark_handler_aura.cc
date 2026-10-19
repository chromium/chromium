// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/tab_contents/web_drag_bookmark_handler_aura.h"

#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"
#include "ui/base/dragdrop/os_exchange_data.h"

using content::WebContents;

WebDragBookmarkHandlerAura::WebDragBookmarkHandlerAura() = default;

WebDragBookmarkHandlerAura::~WebDragBookmarkHandlerAura() = default;

void WebDragBookmarkHandlerAura::DragInitialize(WebContents* contents) {
  // Ideally we would want to initialize the
  // BookmarkManagerPrivateDragEventRouter member in the constructor. We cannot
  // do that as the WebDragDest object is created during the construction of the
  // WebContents object. The BookmarkManagerPrivateDragEventRouter is created
  // much later.
  web_contents_ = contents;
  if (!bookmark_drag_) {
    bookmark_drag_ =
        extensions::BookmarkManagerPrivateDragEventRouter::FromWebContents(
            contents);
  }
}

void WebDragBookmarkHandlerAura::OnDragOver() {
  DCHECK(web_contents_);
  if (bookmark_drag_ && bookmark_drag_data_.is_valid()) {
    bookmark_drag_->OnDragOver(bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerAura::OnReceiveDragData(
    const ui::OSExchangeData& data) {
  if (bookmark_drag_) {
    // Read the bookmark drag data and save it for use in later events in this
    // drag.
    bookmark_drag_data_.Read(data);
  }
}

void WebDragBookmarkHandlerAura::OnDragEnter() {
  if (bookmark_drag_ && bookmark_drag_data_.is_valid()) {
    bookmark_drag_->OnDragEnter(bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerAura::OnDrop() {
  if (bookmark_drag_) {
    if (bookmark_drag_data_.is_valid()) {
      bookmark_drag_->OnDrop(bookmark_drag_data_);
    }

    // Focus the target browser.
    BrowserWindowInterface* browser =
        GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
            web_contents_);
    if (browser) {
      browser->GetWindow()->Show();
    }
  }

  bookmark_drag_data_.Clear();
}

void WebDragBookmarkHandlerAura::OnDragLeave() {
  if (bookmark_drag_ && bookmark_drag_data_.is_valid()) {
    bookmark_drag_->OnDragLeave(bookmark_drag_data_);
  }

  bookmark_drag_data_.Clear();
}
