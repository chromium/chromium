// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/supplemental_device_picker_item.h"

#include "base/unguessable_token.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"
#include "content/public/browser/media_session.h"
#include "services/media_session/public/cpp/media_image_manager.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "ui/gfx/image/image_skia.h"

SupplementalDevicePickerItem::SupplementalDevicePickerItem(
    global_media_controls::MediaItemManager* item_manager,
    const base::UnguessableToken& source_id)
    : id_(base::UnguessableToken::Create().ToString()),
      item_manager_(item_manager),
      source_id_(source_id) {}

SupplementalDevicePickerItem::~SupplementalDevicePickerItem() {
  item_manager_->HideItem(id_);
}

void SupplementalDevicePickerItem::SetView(
    media_message_center::MediaNotificationView* view) {
  view_ = view;
  if (!view_) {
    return;
  }
  UpdateViewWithImages();
  UpdateViewWithMetadata();
}

void SupplementalDevicePickerItem::Dismiss() {
  item_manager_->HideItem(id_);
}

void SupplementalDevicePickerItem::UpdateViewWithMetadata(
    const media_session::MediaMetadata& metadata) {
  metadata_ = metadata;
  UpdateViewWithMetadata();
}

void SupplementalDevicePickerItem::UpdateViewWithArtworkImage(
    std::optional<gfx::ImageSkia> artwork_image) {
  artwork_image_ = artwork_image;
  UpdateViewWithImages();
}

void SupplementalDevicePickerItem::UpdateViewWithFaviconImage(
    std::optional<gfx::ImageSkia> favicon_image) {
  favicon_image_ = favicon_image;
  UpdateViewWithImages();
}

bool SupplementalDevicePickerItem::RequestMediaRemoting() {
  return false;
}

media_message_center::Source SupplementalDevicePickerItem::GetSource() const {
  return media_message_center::Source::kCastDevicePicker;
}

media_message_center::SourceType SupplementalDevicePickerItem::GetSourceType()
    const {
  return media_message_center::SourceType::kPresentationRequest;
}

std::optional<base::UnguessableToken>
SupplementalDevicePickerItem::GetSourceId() const {
  return source_id_;
}

void SupplementalDevicePickerItem::UpdateViewWithMetadata() {
  if (!view_) {
    return;
  }
  view_->UpdateWithMediaMetadata(metadata_);
}

void SupplementalDevicePickerItem::UpdateViewWithImages() {
  if (!view_) {
    return;
  }
  if (favicon_image_) {
    view_->UpdateWithFavicon(*favicon_image_);
    return;
  }
  if (artwork_image_) {
    view_->UpdateWithMediaArtwork(*artwork_image_);
    return;
  }
  view_->UpdateWithFavicon(gfx::ImageSkia());
}
