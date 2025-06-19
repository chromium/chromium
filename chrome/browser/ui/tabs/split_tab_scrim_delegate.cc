// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_scrim_delegate.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"

namespace split_tabs {

SplitTabScrimDelegateImpl::SplitTabScrimDelegateImpl(BrowserView* browser_view)
    : browser_view_(browser_view) {}
SplitTabScrimDelegateImpl::~SplitTabScrimDelegateImpl() = default;

void SplitTabScrimDelegateImpl::ShowScrim() {
  UpdateScrimVisibility(true);
}

void SplitTabScrimDelegateImpl::HideScrim() {
  UpdateScrimVisibility(false);
}

void SplitTabScrimDelegateImpl::UpdateScrimVisibility(bool show_scrim) {
  browser_view_->multi_contents_view()->SetInactiveScrimVisibility(show_scrim);
}

}  // namespace split_tabs
