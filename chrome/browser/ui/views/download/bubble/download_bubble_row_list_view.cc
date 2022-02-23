// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"

DownloadBubbleRowListView::DownloadBubbleRowListView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

BEGIN_METADATA(DownloadBubbleRowListView, views::View)
END_METADATA
