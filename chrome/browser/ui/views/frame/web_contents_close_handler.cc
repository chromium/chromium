// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/web_contents_close_handler.h"

#include "chrome/browser/ui/views/frame/web_contents_close_handler_delegate.h"

WebContentsCloseHandler::WebContentsCloseHandler(
    WebContentsCloseHandlerDelegate* delegate)
    : delegate_(delegate),
      in_close_(false),
      tab_changed_after_clone_(false) {
}

WebContentsCloseHandler::~WebContentsCloseHandler() {
}

void WebContentsCloseHandler::TabInserted() {
  // Tests may end up reviving a TabStrip that is empty.
  if (!in_close_)
    return;
  in_close_ = false;
  delegate_->DestroyClonedLayer();
}

void WebContentsCloseHandler::ActiveTabChanged() {
  if (in_close_)
    tab_changed_after_clone_ = true;
  else
    delegate_->DestroyClonedLayer();
}

void WebContentsCloseHandler::WillCloseAllTabs() {
  DCHECK(!in_close_);
  in_close_ = true;
  tab_changed_after_clone_ = false;
  delegate_->CloneWebContentsLayer();
  timer_.Stop();
}

void WebContentsCloseHandler::CloseAllTabsCanceled() {
  DCHECK(in_close_);
  in_close_ = false;
  if (tab_changed_after_clone_) {
    // If the tab changed, destroy immediately. That way we make sure we aren't
    // showing the wrong thing.
    delegate_->DestroyClonedLayer();
  } else {
    // The most common reason for a close to be canceled is a before unload
    // handler. Often times the tab still ends up closing, but only after we get
    // back a response from the renderer. Assume this is going to happen and
    // keep around the cloned layer for a bit more time.
    timer_.Start(FROM_HERE, base::Milliseconds(500), this,
                 &WebContentsCloseHandler::OnStillHaventClosed);
  }
}

void WebContentsCloseHandler::OnStillHaventClosed() {
  delegate_->DestroyClonedLayer();
}
