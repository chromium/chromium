// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_ITEM_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_ITEM_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "content/public/browser/presentation_request.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class MediaSession;
}  // namespace content

// This object observes changes to the media session related to a device picker
// shown for a presentation request, and forwards those changes to
// global_media_controls::mojom::DevicePickerProvider.
class PresentationRequestNotificationItem final
    : public media_session::mojom::MediaSessionObserver {
 public:
  PresentationRequestNotificationItem(
      const content::PresentationRequest& request,
      std::unique_ptr<media_router::StartPresentationContext> context,
      const mojo::Remote<global_media_controls::mojom::DevicePickerProvider>&
          provider);
  PresentationRequestNotificationItem(
      const PresentationRequestNotificationItem&) = delete;
  PresentationRequestNotificationItem& operator=(
      const PresentationRequestNotificationItem&) = delete;
  ~PresentationRequestNotificationItem() final;

  // Usually, a MediaSessionNotificationItem is shown instead of a
  // PresentationRequestNotificationItem when a user tries to cast from a page
  // that has a media session. However, in certain cases the media session is
  // not active and we show a PresentationRequestNotificationItem instead (e.g.
  // when the user dismisses the MediaSessionNotificationItem).
  //
  // media_session::mojom::MediaSessionObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override {}
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override {}
  void MediaSessionImagesChanged(
      const base::flat_map<media_session::mojom::MediaSessionImageType,
                           std::vector<media_session::MediaImage>>& images)
      override;
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override {}

  base::WeakPtr<PresentationRequestNotificationItem> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  static void SetMediaSessionForTest(content::MediaSession* media_session);

  media_router::StartPresentationContext* context() const {
    return context_.get();
  }
  bool is_default_presentation_request() const {
    return is_default_presentation_request_;
  }

  std::unique_ptr<media_router::StartPresentationContext> PassContext() {
    return std::move(context_);
  }
  const content::PresentationRequest& request() const { return request_; }

 private:
  void UpdatePickerWithMetadata();
  void UpdatePickerWithImages();
  void OnArtworkBitmap(const SkBitmap& bitmap);
  void OnFaviconBitmap(const SkBitmap& bitmap);

  // True if the item is created from a default PresentationRequest, which means
  // |context_| is set to nullptr in the constructor.
  const bool is_default_presentation_request_;

  // |context_| is nullptr if:
  // (1) It is created for a default PresentationRequest;
  // (2) MediaNotificationService has passed |context_| to initialize a
  // CastDialogController.
  std::unique_ptr<media_router::StartPresentationContext> context_;
  const content::PresentationRequest request_;

  mojo::Receiver<media_session::mojom::MediaSessionObserver> observer_receiver_{
      this};

  // The metadata for the Media Session associated with the WebContents that
  // this presentation request is associated with.
  std::optional<media_session::MediaMetadata> metadata_;

  // The favicon/artwork images for the Media Session associated with the
  // WebContents this presentation request is associated with.
  gfx::ImageSkia artwork_image_;
  gfx::ImageSkia favicon_image_;

  const raw_ref<
      const mojo::Remote<global_media_controls::mojom::DevicePickerProvider>>
      provider_;

  base::WeakPtrFactory<PresentationRequestNotificationItem> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_ITEM_H_
