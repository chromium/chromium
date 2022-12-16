// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_list_view.h"

#include "base/containers/contains.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/border.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"

namespace global_media_controls {

namespace {

constexpr int kMediaListMaxHeight = 478;

// Thickness of separator border.
constexpr int kMediaListSeparatorThickness = 2;

std::unique_ptr<views::Border> CreateMediaListSeparatorBorder(SkColor color,
                                                              int thickness) {
  return views::CreateSolidSidedBorder(gfx::Insets::TLBR(thickness, 0, 0, 0),
                                       color);
}

}  // anonymous namespace

MediaItemUIListView::SeparatorStyle::SeparatorStyle(SkColor separator_color,
                                                    int separator_thickness)
    : separator_color(separator_color),
      separator_thickness(separator_thickness) {}

MediaItemUIListView::MediaItemUIListView()
    : MediaItemUIListView(absl::nullopt, /*should_clip_height=*/true) {}

MediaItemUIListView::MediaItemUIListView(
    const absl::optional<SeparatorStyle>& separator_style,
    bool should_clip_height)
    : separator_style_(separator_style) {
  SetBackgroundColor(absl::nullopt);
  SetContents(std::make_unique<views::View>());
  contents()->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  ClipHeightTo(0, should_clip_height ? kMediaListMaxHeight
                                     : std::numeric_limits<int>::max());

  SetVerticalScrollBar(
      std::make_unique<views::OverlayScrollBar>(/*horizontal=*/false));
  SetHorizontalScrollBar(
      std::make_unique<views::OverlayScrollBar>(/*horizontal=*/true));
}

MediaItemUIListView::~MediaItemUIListView() = default;

void MediaItemUIListView::ShowItem(const std::string& id,
                                   std::unique_ptr<MediaItemUIView> item) {
  DCHECK(!base::Contains(items_, id));
  DCHECK_NE(nullptr, item.get());

  // If this isn't the first item, then create a top-sided separator
  // border.
  if (!items_.empty()) {
    if (separator_style_.has_value()) {
      item->SetBorder(CreateMediaListSeparatorBorder(
          separator_style_->separator_color,
          separator_style_->separator_thickness));
    } else {
      item->SetBorder(CreateMediaListSeparatorBorder(
          GetColorProvider()->GetColor(ui::kColorMenuSeparator),
          kMediaListSeparatorThickness));
    }
  }

  item->SetScrollView(this);
  items_[id] = contents()->AddChildView(std::move(item));

  contents()->InvalidateLayout();
  PreferredSizeChanged();
}

void MediaItemUIListView::HideItem(const std::string& id) {
  if (!base::Contains(items_, id))
    return;

  // If we're removing the topmost item and there are others, then we need to
  // remove the top-sided separator border from the new topmost item.
  if (contents()->children().size() > 1 &&
      contents()->children().at(0) == items_[id]) {
    contents()->children().at(1)->SetBorder(nullptr);
  }

  // Remove the item. Note that since |RemoveChildView()| does not delete the
  // item, we now have ownership.
  contents()->RemoveChildView(items_[id]);
  delete items_[id];
  items_.erase(id);

  contents()->InvalidateLayout();
  PreferredSizeChanged();
}

base::WeakPtr<MediaItemUIListView> MediaItemUIListView::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

BEGIN_METADATA(MediaItemUIListView, views::ScrollView)
END_METADATA

}  // namespace global_media_controls
