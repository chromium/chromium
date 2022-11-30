// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/ui_media_sink.h"

namespace media_router {

UIMediaSink::UIMediaSink(mojom::MediaRouteProviderId provider)
    : provider(provider) {}

UIMediaSink::UIMediaSink(const UIMediaSink& other) = default;

UIMediaSink::~UIMediaSink() = default;

}  // namespace media_router
