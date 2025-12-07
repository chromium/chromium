// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_highlight_delegate.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"

namespace split_tabs {

SplitTabHighlightDelegateImpl::SplitTabHighlightDelegateImpl(
    BrowserView* browser_view)
    : browser_view_(browser_view) {}
SplitTabHighlightDelegateImpl::~SplitTabHighlightDelegateImpl() = default;

void SplitTabHighlightDelegateImpl::SetHighlight(bool is_highlighted) {
  browser_view_->multi_contents_view()->SetHighlightActiveContentsView(
      is_highlighted);
}

}  // namespace split_tabs
