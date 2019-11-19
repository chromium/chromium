// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/tab_contents/web_drag_bookmark_handler_aura.h"

#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/dragdrop/os_exchange_data.h"

using content::WebContents;

WebDragBookmarkHandlerAura::WebDragBookmarkHandlerAura()
    : bookmark_tab_helper_(NULL),
      web_contents_(NULL) {
}

WebDragBookmarkHandlerAura::~WebDragBookmarkHandlerAura() {
}

void WebDragBookmarkHandlerAura::DragInitialize(WebContents* contents) {
  // Ideally we would want to initialize the the BookmarkTabHelper member in
  // the constructor. We cannot do that as the WebDragDest object is
  // created during the construction of the WebContents object.  The
  // BookmarkTabHelper is created much later.
  web_contents_ = contents;
  if (!bookmark_tab_helper_)
    bookmark_tab_helper_ = BookmarkTabHelper::FromWebContents(contents);
}

void WebDragBookmarkHandlerAura::OnDragOver() {
  DCHECK(web_contents_);
  if (bookmark_tab_helper_ && bookmark_tab_helper_->bookmark_drag_delegate()) {
    if (bookmark_drag_data_.is_valid())
      bookmark_tab_helper_->bookmark_drag_delegate()->OnDragOver(
          bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerAura::OnReceiveDragData(
    const ui::OSExchangeData& data) {
  if (bookmark_tab_helper_ && bookmark_tab_helper_->bookmark_drag_delegate()) {
    // Read the bookmark drag data and save it for use in later events in this
    // drag.
    bookmark_drag_data_.Read(data);
  }
}

void WebDragBookmarkHandlerAura::OnDragEnter() {
  if (bookmark_tab_helper_ && bookmark_tab_helper_->bookmark_drag_delegate()) {
    if (bookmark_drag_data_.is_valid())
      bookmark_tab_helper_->bookmark_drag_delegate()->OnDragEnter(
          bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerAura::OnDrop() {
  if (bookmark_tab_helper_) {
    if (bookmark_tab_helper_->bookmark_drag_delegate()) {
      if (bookmark_drag_data_.is_valid()) {
        bookmark_tab_helper_->bookmark_drag_delegate()->OnDrop(
            bookmark_drag_data_);
      }
    }

    // Focus the target browser.
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
    if (browser)
      browser->window()->Show();
  }

  bookmark_drag_data_.Clear();
}

void WebDragBookmarkHandlerAura::OnDragLeave() {
  if (bookmark_tab_helper_ && bookmark_tab_helper_->bookmark_drag_delegate()) {
    if (bookmark_drag_data_.is_valid())
      bookmark_tab_helper_->bookmark_drag_delegate()->OnDragLeave(
          bookmark_drag_data_);
  }

  bookmark_drag_data_.Clear();
}
