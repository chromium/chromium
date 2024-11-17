// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/presentation/presentation_media_sinks_observer.h"

#include "components/media_router/browser/media_router.h"
#include "components/media_router/common/media_source.h"
#include "content/public/browser/presentation_screen_availability_listener.h"

namespace media_router {

PresentationMediaSinksObserver::PresentationMediaSinksObserver(
    MediaRouter* router,
    content::PresentationScreenAvailabilityListener* listener,
    const MediaSource& source,
    const url::Origin& origin)
    : MediaSinksObserver(router, source, origin),
      listener_(listener),
      previous_availability_(blink::mojom::ScreenAvailability::UNKNOWN) {
  DCHECK(router);
  DCHECK(listener_);
}

PresentationMediaSinksObserver::~PresentationMediaSinksObserver() = default;

void PresentationMediaSinksObserver::OnSinksReceived(
    const std::vector<MediaSink>& result) {
  blink::mojom::ScreenAvailability current_availability =
      result.empty() ? blink::mojom::ScreenAvailability::UNAVAILABLE
                     : blink::mojom::ScreenAvailability::AVAILABLE;
  // Don't send if new result is same as previous.
  if (previous_availability_ == current_availability)
    return;

  listener_->OnScreenAvailabilityChanged(current_availability);
  previous_availability_ = current_availability;
}

}  // namespace media_router
