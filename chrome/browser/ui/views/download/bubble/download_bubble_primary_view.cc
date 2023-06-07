// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_primary_view.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/layout_types.h"

DownloadBubblePrimaryView::DownloadBubblePrimaryView()
    : creation_time_(base::Time::Now()) {
  SetOrientation(views::LayoutOrientation::kVertical);
  SetNotifyEnterExitOnChild(true);
}

DownloadBubblePrimaryView::~DownloadBubblePrimaryView() = default;

void DownloadBubblePrimaryView::LogVisibleTimeMetrics() const {
  base::StringPiece histogram_name = GetVisibleTimeHistogramName();
  if (!histogram_name.empty()) {
    base::UmaHistogramMediumTimes(std::string(histogram_name),
                                  base::Time::Now() - creation_time_);
  }
}

void DownloadBubblePrimaryView::BuildAndAddScrollView(
    base::WeakPtr<Browser> browser,
    base::WeakPtr<DownloadBubbleUIController> bubble_controller,
    base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
    std::vector<DownloadUIModel::DownloadUIModelPtr> models,
    int fixed_width) {
  // TODO(crbug.com/1450660): Actually construct the ScrollView here.
  scroll_view_ = AddChildView(DownloadBubbleRowListView::CreateWithScroll(
      std::move(browser), std::move(bubble_controller),
      std::move(navigation_handler), std::move(models), fixed_width));
}

int DownloadBubblePrimaryView::DefaultPreferredWidth() const {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
}

BEGIN_METADATA(DownloadBubblePrimaryView, views::FlexLayoutView)
END_METADATA
