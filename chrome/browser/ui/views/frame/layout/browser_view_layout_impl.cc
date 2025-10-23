// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"

// BrowserViewLayoutImpl (New Implementation)

BrowserViewLayoutImpl::BrowserViewLayoutImpl(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    Browser* browser,
    BrowserViewLayoutViews views)
    : BrowserViewLayout(std::move(delegate), browser, std::move(views)) {}
BrowserViewLayoutImpl::~BrowserViewLayoutImpl() = default;

void BrowserViewLayoutImpl::Layout(views::View* host) {
  NOTIMPLEMENTED();
}
gfx::Size BrowserViewLayoutImpl::GetMinimumSize(const views::View* host) const {
  NOTIMPLEMENTED();
  return gfx::Size();
}
