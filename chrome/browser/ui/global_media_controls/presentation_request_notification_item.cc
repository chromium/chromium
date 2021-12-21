// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_item.h"

#include "base/unguessable_token.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"
#include "components/url_formatter/elide_url.h"
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
    global_media_controls::MediaItemManager* item_manager,
    const content::PresentationRequest& request,
    std::unique_ptr<media_router::StartPresentationContext> context)
    : id_(base::UnguessableToken::Create().ToString()),
      item_manager_(item_manager),
      is_default_presentation_request_(context == nullptr),
      context_(std::move(context)),
      request_(request) {
  DCHECK(!context_ || request == context_->presentation_request());
}

PresentationRequestNotificationItem::~PresentationRequestNotificationItem() {
  item_manager_->HideItem(id_);
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
  if (favicon_driver) {
    view_->UpdateWithFavicon(favicon_driver->GetFavicon().AsImageSkia());
  }

  media_session::MediaMetadata data;
  data.source_title = url_formatter::FormatOriginForSecurityDisplay(
      request_.frame_origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  data.artist = web_contents->GetTitle();

  view_->UpdateWithMediaMetadata(data);
}

void PresentationRequestNotificationItem::OnMediaSessionActionButtonPressed(
    media_session::mojom::MediaSessionAction action) {}

void PresentationRequestNotificationItem::Dismiss() {
  item_manager_->HideItem(id_);
}

media_message_center::SourceType
PresentationRequestNotificationItem::SourceType() {
  return media_message_center::SourceType::kPresentationRequest;
}
