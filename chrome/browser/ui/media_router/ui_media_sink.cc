// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/ui_media_sink.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace media_router {

UIMediaSink::UIMediaSink(mojom::MediaRouteProviderId provider)
    : provider(provider) {}

UIMediaSink::UIMediaSink(const UIMediaSink& other) = default;

UIMediaSink::~UIMediaSink() = default;

std::u16string UIMediaSink::GetStatusTextForDisplay() const {
  if (issue) {
    return base::UTF8ToUTF16(issue->info().title);
  }
  // If the sink is disconnecting, say so instead of using the source info
  // stored in `status_text`.
  if (state == UIMediaSinkState::DISCONNECTING) {
    return l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_DISCONNECTING);
  }
  if (freeze_info.is_frozen) {
    return l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_PAUSED);
  }
  if (!status_text.empty()) {
    return status_text;
  }
  switch (state) {
    case UIMediaSinkState::AVAILABLE:
      return l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_AVAILABLE);
    case UIMediaSinkState::CONNECTING:
      return l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_CONNECTING);
    default:
      return std::u16string();
  }
}

}  // namespace media_router
