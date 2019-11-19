// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/web_drag_bookmark_handler_mac.h"

#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

WebDragBookmarkHandlerMac::WebDragBookmarkHandlerMac()
    : bookmark_tab_helper_(NULL),
      web_contents_(NULL) {
}

WebDragBookmarkHandlerMac::~WebDragBookmarkHandlerMac() {}

void WebDragBookmarkHandlerMac::DragInitialize(WebContents* contents) {
  web_contents_ = contents;
  if (!bookmark_tab_helper_)
    bookmark_tab_helper_ = BookmarkTabHelper::FromWebContents(contents);

  bookmark_drag_data_.ReadFromClipboard(ui::ClipboardBuffer::kDrag);
}

void WebDragBookmarkHandlerMac::OnDragOver() {
  if (bookmark_tab_helper_ && bookmark_tab_helper_->bookmark_drag_delegate()) {
    bookmark_tab_helper_->bookmark_drag_delegate()->OnDragOver(
        bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerMac::OnDragEnter() {
  if (bookmark_tab_helper_ && bookmark_tab_helper_->bookmark_drag_delegate()) {
    bookmark_tab_helper_->bookmark_drag_delegate()->OnDragEnter(
        bookmark_drag_data_);
  }
}

void WebDragBookmarkHandlerMac::OnDrop() {
  // This is non-null if the web_contents_ is showing an ExtensionWebUI with
  // support for (at the moment experimental) drag and drop extensions.
  if (bookmark_tab_helper_) {
    if (bookmark_tab_helper_->bookmark_drag_delegate()) {
      bookmark_tab_helper_->bookmark_drag_delegate()->OnDrop(
          bookmark_drag_data_);
    }

    // Focus the target browser.
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
    if (browser)
      browser->window()->Show();
  }
}

void WebDragBookmarkHandlerMac::OnDragLeave() {
  if (bookmark_tab_helper_ && bookmark_tab_helper_->bookmark_drag_delegate()) {
    bookmark_tab_helper_->bookmark_drag_delegate()->OnDragLeave(
        bookmark_drag_data_);
  }
}
