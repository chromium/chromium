// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_list_view.h"

#include "base/containers/contains.h"
#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/border.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"

namespace global_media_controls {

namespace {

constexpr int kMediaListMaxHeight = 488;

// Thickness of separator border.
constexpr int kMediaListSeparatorThickness = 2;

#if !BUILDFLAG(IS_CHROMEOS)
// Padding for the borders and separators for non-CrOS updated UI.
constexpr int kMediaListUpdatedPadding = 8;
#endif

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
    : MediaItemUIListView(std::nullopt, /*should_clip_height=*/true) {}

MediaItemUIListView::MediaItemUIListView(
    const std::optional<SeparatorStyle>& separator_style,
    bool should_clip_height)
    : separator_style_(separator_style) {
  SetBackgroundColor(std::nullopt);
  SetContents(std::make_unique<views::View>());
  contents()->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  ClipHeightTo(0, should_clip_height ? kMediaListMaxHeight
                                     : std::numeric_limits<int>::max());

  SetVerticalScrollBar(std::make_unique<views::OverlayScrollBar>(
      views::ScrollBar::Orientation::kVertical));
  SetHorizontalScrollBar(std::make_unique<views::OverlayScrollBar>(
      views::ScrollBar::Orientation::kHorizontal));

#if !BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsUpdatedUI)) {
    auto* layout =
        static_cast<views::BoxLayout*>(contents()->GetLayoutManager());
    layout->set_inside_border_insets(
        gfx::Insets::VH(kMediaListUpdatedPadding, kMediaListUpdatedPadding));
    layout->set_between_child_spacing(kMediaListUpdatedPadding);
  }
#endif
}

MediaItemUIListView::~MediaItemUIListView() = default;

void MediaItemUIListView::ShowItem(const std::string& id,
                                   std::unique_ptr<MediaItemUIView> item) {
  DCHECK(!base::Contains(items_, id));
  DCHECK_NE(nullptr, item.get());

  bool use_updated_ui = true;
#if !BUILDFLAG(IS_CHROMEOS)
  use_updated_ui =
      base::FeatureList::IsEnabled(media::kGlobalMediaControlsUpdatedUI);
#endif

  // If this isn't the first item, then create a top-sided separator border.
  // No separator border should be drawn for the Chrome OS updated UI.
  if (!items_.empty() && !use_updated_ui) {
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

  contents()->RemoveChildViewT(items_[id]);
  items_.erase(id);

  contents()->InvalidateLayout();
  PreferredSizeChanged();
}

MediaItemUIView* MediaItemUIListView::GetItem(const std::string& id) {
  return items_[id];
}

void MediaItemUIListView::ShowUpdatedItem(
    const std::string& id,
    std::unique_ptr<MediaItemUIUpdatedView> item) {
  CHECK(!base::Contains(updated_items_, id));
  CHECK(item.get());

  updated_items_[id] = contents()->AddChildView(std::move(item));

  contents()->InvalidateLayout();
  PreferredSizeChanged();
}

void MediaItemUIListView::HideUpdatedItem(const std::string& id) {
  if (!base::Contains(updated_items_, id)) {
    return;
  }

  contents()->RemoveChildViewT(updated_items_[id]);
  updated_items_.erase(id);

  contents()->InvalidateLayout();
  PreferredSizeChanged();
}

MediaItemUIUpdatedView* MediaItemUIListView::GetUpdatedItem(
    const std::string& id) {
  return updated_items_[id];
}

base::WeakPtr<MediaItemUIListView> MediaItemUIListView::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

BEGIN_METADATA(MediaItemUIListView)
END_METADATA

}  // namespace global_media_controls
