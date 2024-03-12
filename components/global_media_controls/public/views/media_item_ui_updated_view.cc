// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"

#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

namespace global_media_controls {

namespace {

constexpr gfx::Insets kBackgroundInsets = gfx::Insets::TLBR(16, 8, 8, 8);
constexpr gfx::Insets kMainRowInsets = gfx::Insets::TLBR(0, 8, 8, 8);
constexpr gfx::Insets kMediaInfoInsets = gfx::Insets::TLBR(0, 16, 0, 4);
constexpr gfx::Insets kSourceRowInsets = gfx::Insets::TLBR(0, 0, 6, 0);

constexpr int kBackgroundCornerRadius = 16;
constexpr int kArtworkCornerRadius = 12;
constexpr int kMediaInfoSeparator = 4;

constexpr float kFocusRingHaloInset = -3.0f;

constexpr gfx::Size kBackgroundSize = gfx::Size(400, 150);
constexpr gfx::Size kArtworkSize = gfx::Size(74, 74);

// If the image does not fit the square view, scale the image to fill the view
// even if part of the image is cropped.
gfx::Size ScaleImageSizeToFitView(const gfx::Size& image_size,
                                  const gfx::Size& view_size) {
  const float scale =
      std::max(view_size.width() / static_cast<float>(image_size.width()),
               view_size.height() / static_cast<float>(image_size.height()));
  return gfx::ScaleToFlooredSize(image_size, scale);
}

}  // namespace

MediaItemUIUpdatedView::MediaItemUIUpdatedView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    media_message_center::MediaColorTheme media_color_theme)
    : id_(id), item_(std::move(item)), media_color_theme_(media_color_theme) {
  CHECK(item_);

  SetPreferredSize(kBackgroundSize);
  SetBackground(views::CreateThemedRoundedRectBackground(
      media_color_theme_.background_color_id, kBackgroundCornerRadius));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kBackgroundInsets));

  views::FocusRing::Install(this);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kBackgroundCornerRadius);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHaloInset(kFocusRingHaloInset);
  focus_ring->SetColorId(media_color_theme_.focus_ring_color_id);

  // |main_row| holds everything above the progress view, including the media
  // artwork, media information column and the play/pause button column.
  auto* main_row = AddChildView(std::make_unique<views::BoxLayoutView>());
  main_row->SetInsideBorderInsets(kMainRowInsets);

  artwork_view_ = main_row->AddChildView(std::make_unique<views::ImageView>());
  artwork_view_->SetPreferredSize(kArtworkSize);

  // |media_info_column| inside |main_row| holds the media source, title, and
  // artist.
  auto* media_info_column =
      main_row->AddChildView(std::make_unique<views::BoxLayoutView>());
  media_info_column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  media_info_column->SetInsideBorderInsets(kMediaInfoInsets);
  media_info_column->SetBetweenChildSpacing(kMediaInfoSeparator);
  media_info_column->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  main_row->SetFlexForView(media_info_column, 1);

  // Create the media source label.
  auto* source_row =
      media_info_column->AddChildView(std::make_unique<views::BoxLayoutView>());
  source_row->SetInsideBorderInsets(kSourceRowInsets);

  source_label_ = source_row->AddChildView(std::make_unique<views::Label>(
      u"origin.com", views::style::CONTEXT_LABEL, views::style::STYLE_BODY_5));
  source_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  source_row->SetFlexForView(source_label_, 1);

  item_->SetView(this);
}

MediaItemUIUpdatedView::~MediaItemUIUpdatedView() {
  if (item_) {
    item_->SetView(nullptr);
  }
  for (auto& observer : observers_) {
    observer.OnMediaItemUIDestroyed(id_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// views::View implementations:

void MediaItemUIUpdatedView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kListItem;
  node_data->SetNameChecked(l10n_util::GetStringUTF8(
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACCESSIBLE_NAME));
}

///////////////////////////////////////////////////////////////////////////////
// MediaItemUI implementations:

void MediaItemUIUpdatedView::AddObserver(MediaItemUIObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaItemUIUpdatedView::RemoveObserver(MediaItemUIObserver* observer) {
  observers_.RemoveObserver(observer);
}

///////////////////////////////////////////////////////////////////////////////
// media_message_center::MediaNotificationView implementations:

void MediaItemUIUpdatedView::UpdateWithMediaSessionInfo(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {}

void MediaItemUIUpdatedView::UpdateWithMediaMetadata(
    const media_session::MediaMetadata& metadata) {}

void MediaItemUIUpdatedView::UpdateWithMediaActions(
    const base::flat_set<media_session::mojom::MediaSessionAction>& actions) {}

void MediaItemUIUpdatedView::UpdateWithMediaPosition(
    const media_session::MediaPosition& position) {}

void MediaItemUIUpdatedView::UpdateWithMediaArtwork(
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // Hide the image so the other contents will adjust to fill the container.
    artwork_view_->SetVisible(false);
  } else {
    artwork_view_->SetVisible(true);
    artwork_view_->SetImageSize(
        ScaleImageSizeToFitView(image.size(), kArtworkSize));
    artwork_view_->SetImage(ui::ImageModel::FromImageSkia(image));

    // Draw the image with rounded corners.
    auto path = SkPath().addRoundRect(
        RectToSkRect(gfx::Rect(kArtworkSize.width(), kArtworkSize.height())),
        kArtworkCornerRadius, kArtworkCornerRadius);
    artwork_view_->SetClipPath(path);
  }
  SchedulePaint();
}

BEGIN_METADATA(MediaItemUIUpdatedView)
END_METADATA

}  // namespace global_media_controls
