// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_list_view.h"

#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"

namespace global_media_controls {

namespace {

constexpr int kMediaListMaxHeight = 488;

#if !BUILDFLAG(IS_CHROMEOS)
// Padding for the borders and separators for non-CrOS updated UI.
constexpr int kMediaListUpdatedPadding = 8;
#endif

}  // anonymous namespace

MediaItemUIListView::MediaItemUIListView()
    : MediaItemUIListView(/*should_clip_height=*/true) {}

MediaItemUIListView::MediaItemUIListView(bool should_clip_height) {
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
  auto* layout = static_cast<views::BoxLayout*>(contents()->GetLayoutManager());
  layout->set_inside_border_insets(
      gfx::Insets::VH(kMediaListUpdatedPadding, kMediaListUpdatedPadding));
  layout->set_between_child_spacing(kMediaListUpdatedPadding);
#endif
}

MediaItemUIListView::~MediaItemUIListView() = default;

void MediaItemUIListView::ShowItem(const std::string& id,
                                   std::unique_ptr<MediaItemUIView> item) {
  DCHECK(!items_.contains(id));
  DCHECK_NE(nullptr, item.get());

  item->SetScrollView(this);
  items_[id] = contents()->AddChildView(std::move(item));

  contents()->InvalidateLayout();
  PreferredSizeChanged();
}

void MediaItemUIListView::HideItem(const std::string& id) {
  if (!items_.contains(id))
    return;

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
  CHECK(!updated_items_.contains(id));
  CHECK(item.get());

  updated_items_[id] = contents()->AddChildView(std::move(item));

  contents()->InvalidateLayout();
  PreferredSizeChanged();
}

void MediaItemUIListView::HideUpdatedItem(const std::string& id) {
  if (!updated_items_.contains(id)) {
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
