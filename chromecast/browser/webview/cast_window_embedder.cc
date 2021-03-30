// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/cast_window_embedder.h"

namespace chromecast {

CastWindowEmbedder::EmbedderWindowEvent::EmbedderWindowEvent() = default;
CastWindowEmbedder::EmbedderWindowEvent::~EmbedderWindowEvent() = default;

CastWindowEmbedder::CastWindowProperties::CastWindowProperties() = default;
CastWindowEmbedder::CastWindowProperties::~CastWindowProperties() = default;
CastWindowEmbedder::CastWindowProperties::CastWindowProperties(
    CastWindowProperties&& other) = default;

}  // namespace chromecast
