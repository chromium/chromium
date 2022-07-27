// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"
#include "ui/views/layout/box_layout.h"

PageInfoCookiesContentView::PageInfoCookiesContentView(PageInfo* presenter)
    : presenter_(presenter) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  presenter_->InitializeUiState(this, base::DoNothing());
}

PageInfoCookiesContentView::~PageInfoCookiesContentView() = default;
