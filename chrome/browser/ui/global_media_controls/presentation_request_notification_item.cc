// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_item.h"

#include "base/unguessable_token.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"
#include "components/url_formatter/url_formatter.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "ui/gfx/image/image_skia.h"

content::WebContents* GetWebContentsFromPresentationRequest(
    const content::PresentationRequest& request) {
  auto* rfh = content::RenderFrameHost::FromID(request.render_frame_host_id);
  DCHECK(rfh);
  return content::WebContents::FromRenderFrameHost(rfh);
}

PresentationRequestNotificationItem::PresentationRequestNotificationItem(
    MediaNotificationService* notification_service,
    const content::PresentationRequest& request,
    std::unique_ptr<media_router::StartPresentationContext> context)
    : id_(base::UnguessableToken::Create().ToString()),
      notification_service_(notification_service),
      context_(std::move(context)),
      request_(request) {
  DCHECK(!context || request == context->presentation_request());
}

PresentationRequestNotificationItem::~PresentationRequestNotificationItem() {
  notification_service_->RemoveItem(id_);
}

void PresentationRequestNotificationItem::SetView(
    media_message_center::MediaNotificationView* view) {
  view_ = view;
  if (!view_)
    return;

  auto* web_contents = GetWebContentsFromPresentationRequest(request_);
  DCHECK(web_contents);
  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents);

  media_session::MediaMetadata data;
  data.source_title = url_formatter::FormatUrl(
      web_contents->GetVisibleURL().GetOrigin(),
      url_formatter::kFormatUrlOmitUsernamePassword |
          url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains,
      net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  data.artist = web_contents->GetTitle();

  view_->UpdateWithMediaMetadata(data);
  view_->UpdateWithFavicon(favicon_driver->GetFavicon().AsImageSkia());
}

void PresentationRequestNotificationItem::OnMediaSessionActionButtonPressed(
    media_session::mojom::MediaSessionAction action) {}

void PresentationRequestNotificationItem::Dismiss() {
  notification_service_->HideNotification(id_);
}

media_message_center::SourceType
PresentationRequestNotificationItem::SourceType() {
  return media_message_center::SourceType::kPresentationRequest;
}
