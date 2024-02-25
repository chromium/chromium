// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_item.h"

#include "base/unguessable_token.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"
#include "content/public/browser/media_session.h"
#include "services/media_session/public/cpp/media_image_manager.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "ui/gfx/image/image_skia.h"

namespace {

content::MediaSession* g_media_session_for_test = nullptr;

content::WebContents* GetWebContentsFromPresentationRequest(
    const content::PresentationRequest& request) {
  auto* rfh = content::RenderFrameHost::FromID(request.render_frame_host_id);
  return content::WebContents::FromRenderFrameHost(rfh);
}

std::optional<gfx::ImageSkia> GetCorrectColorTypeImage(const SkBitmap& bitmap) {
  if (bitmap.info().colorType() == kN32_SkColorType) {
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }
  SkImageInfo color_type_info = bitmap.info().makeColorType(kN32_SkColorType);
  SkBitmap color_type_copy;
  if (!color_type_copy.tryAllocPixels(color_type_info)) {
    return std::nullopt;
  }
  if (!bitmap.readPixels(color_type_info, color_type_copy.getPixels(),
                         color_type_copy.rowBytes(), 0, 0)) {
    return std::nullopt;
  }
  return gfx::ImageSkia::CreateFrom1xBitmap(color_type_copy);
}

content::MediaSession* GetMediaSession(content::WebContents* web_contents) {
  if (g_media_session_for_test) {
    return g_media_session_for_test;
  }
  return content::MediaSession::Get(web_contents);
}

}  // namespace

PresentationRequestNotificationItem::PresentationRequestNotificationItem(
    const content::PresentationRequest& request,
    std::unique_ptr<media_router::StartPresentationContext> context,
    const mojo::Remote<global_media_controls::mojom::DevicePickerProvider>&
        provider)
    : is_default_presentation_request_(context == nullptr),
      context_(std::move(context)),
      request_(request),
      provider_(provider) {
  DCHECK(!context_ || request == context_->presentation_request());

  // We want to observe the content::MediaSession associated with the
  // WebContents, even when the content::MediaSession has never requested audio
  // focus through the Media Session API. Since it may not have ever requested
  // audio focus, it isn't observable the usual way (through the Media Session
  // API). Therefore, we observe the content::MediaSession directly though the
  // content API. Note that the content::MediaSession always exists, even when
  // the page has no media players.
  auto* web_contents = GetWebContentsFromPresentationRequest(request_);
  if (!web_contents) {
    return;
  }
  auto* media_session = GetMediaSession(web_contents);
  DCHECK(media_session);
  media_session->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
}

PresentationRequestNotificationItem::~PresentationRequestNotificationItem() {
  if (provider_->is_bound()) {
    (*provider_)->HideItem();
  }
}

void PresentationRequestNotificationItem::MediaSessionMetadataChanged(
    const std::optional<media_session::MediaMetadata>& metadata) {
  metadata_ = metadata;
  UpdatePickerWithMetadata();
}

void PresentationRequestNotificationItem::MediaSessionImagesChanged(
    const base::flat_map<media_session::mojom::MediaSessionImageType,
                         std::vector<media_session::MediaImage>>& images) {
  auto* web_contents = GetWebContentsFromPresentationRequest(request_);
  if (!web_contents) {
    return;
  }

  auto* media_session = GetMediaSession(web_contents);
  DCHECK(media_session);
  media_session::MediaImageManager manager(
      global_media_controls::kMediaItemArtworkMinSize,
      global_media_controls::kMediaItemArtworkDesiredSize);
  bool should_synchronously_update_picker = false;

  std::optional<media_session::MediaImage> artwork_image;
  auto it = images.find(media_session::mojom::MediaSessionImageType::kArtwork);
  if (it == images.end()) {
    artwork_image = std::nullopt;
  } else {
    artwork_image = manager.SelectImage(it->second);
  }

  if (artwork_image) {
    media_session->GetMediaImageBitmap(
        *artwork_image, global_media_controls::kMediaItemArtworkMinSize,
        global_media_controls::kMediaItemArtworkDesiredSize,
        base::BindOnce(&PresentationRequestNotificationItem::OnArtworkBitmap,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    artwork_image_ = gfx::ImageSkia();
    should_synchronously_update_picker = true;
  }

  std::optional<media_session::MediaImage> favicon_image;
  it = images.find(media_session::mojom::MediaSessionImageType::kSourceIcon);
  if (it == images.end()) {
    favicon_image = std::nullopt;
  } else {
    favicon_image = manager.SelectImage(it->second);
  }
  if (favicon_image) {
    media_session->GetMediaImageBitmap(
        *favicon_image, global_media_controls::kMediaItemArtworkMinSize,
        global_media_controls::kMediaItemArtworkDesiredSize,
        base::BindOnce(&PresentationRequestNotificationItem::OnFaviconBitmap,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    favicon_image_ = gfx::ImageSkia();
    should_synchronously_update_picker = true;
  }
  if (should_synchronously_update_picker) {
    UpdatePickerWithImages();
  }
}

// static
void PresentationRequestNotificationItem::SetMediaSessionForTest(
    content::MediaSession* media_session) {
  g_media_session_for_test = media_session;
}

void PresentationRequestNotificationItem::UpdatePickerWithMetadata() {
  if (!provider_->is_bound()) {
    return;
  }
  // If we have metadata from the media session, use that.
  media_session::MediaMetadata data =
      metadata_.value_or(media_session::MediaMetadata{});

  if (media_message_center::IsOriginGoodForDisplay(request_.frame_origin)) {
    // `request_` has more accurate origin info than `metadata_` e.g. when the
    // request is from within an iframe.
    data.source_title =
        media_message_center::GetOriginNameForDisplay(request_.frame_origin);
  }

  auto* web_contents = GetWebContentsFromPresentationRequest(request_);
  // If not empty, then `metadata_.artist` is likely to contain information
  // more relevant than the page title.
  if (web_contents && data.artist.empty()) {
    data.artist = web_contents->GetTitle();
  }
  (*provider_)->OnMetadataChanged(data);
}

void PresentationRequestNotificationItem::UpdatePickerWithImages() {
  if (!provider_->is_bound()) {
    return;
  }
  (*provider_)->OnArtworkImageChanged(artwork_image_);
  if (!favicon_image_.isNull()) {
    (*provider_)->OnFaviconImageChanged(favicon_image_);
    return;
  }
  // Otherwise, get one ourselves.
  auto* web_contents = GetWebContentsFromPresentationRequest(request_);
  if (web_contents) {
    favicon::FaviconDriver* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(web_contents);
    if (favicon_driver) {
      (*provider_)
          ->OnFaviconImageChanged(favicon_driver->GetFavicon().AsImageSkia());
      return;
    }
  }
  (*provider_)->OnFaviconImageChanged(gfx::ImageSkia());
}

void PresentationRequestNotificationItem::OnArtworkBitmap(
    const SkBitmap& bitmap) {
  artwork_image_ = GetCorrectColorTypeImage(bitmap).value_or(gfx::ImageSkia());
  UpdatePickerWithImages();
}

void PresentationRequestNotificationItem::OnFaviconBitmap(
    const SkBitmap& bitmap) {
  favicon_image_ = GetCorrectColorTypeImage(bitmap).value_or(gfx::ImageSkia());
  UpdatePickerWithImages();
}
