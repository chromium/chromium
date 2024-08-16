// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_primary_view.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/download/download_bubble_info.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/vector_icons.h"

namespace {

// 7.5 rows * 60 px per row = 450;
constexpr int kMaxHeightForRowList = 450;

bool IsOtrInfoRowEnabled(Browser* browser) {
  if (!browser || !browser->profile()) {
    return false;
  }
  return browser->profile()->IsOffTheRecord();
}

}  // namespace

DownloadBubblePrimaryView::DownloadBubblePrimaryView()
    : creation_time_(base::Time::Now()) {
  SetOrientation(views::LayoutOrientation::kVertical);
  SetNotifyEnterExitOnChild(true);
}

DownloadBubblePrimaryView::~DownloadBubblePrimaryView() = default;

void DownloadBubblePrimaryView::LogVisibleTimeMetrics() const {
  std::string_view histogram_name = GetVisibleTimeHistogramName();
  if (!histogram_name.empty()) {
    base::UmaHistogramMediumTimes(histogram_name,
                                  base::Time::Now() - creation_time_);
  }
}

void DownloadBubblePrimaryView::BuildAndAddScrollView(
    base::WeakPtr<Browser> browser,
    base::WeakPtr<DownloadBubbleUIController> bubble_controller,
    base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
    const DownloadBubbleRowListViewInfo& info,
    int fixed_width) {
  auto row_list_view = std::make_unique<DownloadBubbleRowListView>(
      browser, bubble_controller, navigation_handler, fixed_width, info,
      IsPartialView());
  row_list_view_ = row_list_view.get();
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->SetContents(std::move(row_list_view));
  scroll_view_->ClipHeightTo(0, kMaxHeightForRowList);
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
}

void DownloadBubblePrimaryView::MaybeAddOtrInfoRow(Browser* browser) {
  if (!IsOtrInfoRowEnabled(browser)) {
    return;
  }
  auto* header_info_row =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  header_info_row->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  header_info_row->SetBetweenChildSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  header_info_row->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  header_info_row->SetBorder(
      views::CreateEmptyBorder(GetLayoutInsets(DOWNLOAD_ROW)));
  header_info_row->SetBackground(views::CreateThemedRoundedRectBackground(
      kColorDownloadBubbleInfoBackground,
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh)));

  auto* info_icon =
      header_info_row->AddChildView(std::make_unique<views::ImageView>());
  info_icon->SetBorder(
      views::CreateEmptyBorder(GetLayoutInsets(DOWNLOAD_ICON)));
  info_icon->SetImage(ui::ImageModel::FromVectorIcon(
      views::kInfoIcon, kColorDownloadBubbleInfoIcon,
      GetLayoutConstant(DOWNLOAD_ICON_SIZE)));

  auto* info_label =
      header_info_row->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(
              IDS_DOWNLOAD_BUBBLE_INCOGNITO_INFORMATION_ROW),
          views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  info_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  info_label->SetMultiLine(true);

  // As noted in https://crbug.com/1340937#c3, the layout
  // seems to have an issue with multi-line labels. As a workaround, give the
  // label the fixed size width.
  const int side_margin = GetLayoutInsets(DOWNLOAD_ROW).width();
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  const int min_label_width =
      bubble_width - side_margin - GetLayoutConstant(DOWNLOAD_ICON_SIZE) -
      GetLayoutInsets(DOWNLOAD_ICON).width() - icon_label_spacing;
  info_label->SizeToFit(min_label_width);
}

int DownloadBubblePrimaryView::DefaultPreferredWidth() const {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
}

DownloadBubbleRowView* DownloadBubblePrimaryView::GetRow(
    const offline_items_collection::ContentId& id) {
  return row_list_view_->GetRow(id);
}

views::View* DownloadBubblePrimaryView::GetInitiallyFocusedView() {
  if (row_list_view_->children().empty()) {
    return nullptr;
  }
  return static_cast<DownloadBubbleRowView*>(row_list_view_->children().front())
      ->transparent_button();
}

DownloadBubbleRowView* DownloadBubblePrimaryView::GetRowForTesting(
    size_t index) {
  return static_cast<DownloadBubbleRowView*>(row_list_view_->children()[index]);
}

BEGIN_METADATA(DownloadBubblePrimaryView)
END_METADATA
