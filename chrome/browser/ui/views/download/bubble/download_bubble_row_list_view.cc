// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "ui/base/metadata/metadata_impl_macros.h"

DownloadBubbleRowListView::DownloadBubbleRowListView(bool is_partial_view)
    : is_partial_view_(is_partial_view), creation_time_(base::Time::Now()) {
  SetOrientation(views::LayoutOrientation::kVertical);
}

DownloadBubbleRowListView::~DownloadBubbleRowListView() {
  base::UmaHistogramMediumTimes(
      base::StrCat({"Download.Bubble.", is_partial_view_ ? "Partial" : "Full",
                    "View.VisibleTime"}),
      base::Time::Now() - creation_time_);
}

BEGIN_METADATA(DownloadBubbleRowListView, views::FlexLayoutView)
END_METADATA
