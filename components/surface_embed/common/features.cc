// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surface_embed/common/features.h"

namespace surface_embed {

namespace features {

// Enable the SurfaceEmbed mechanism for embedding WebContents via a WebPlugin.
// https://docs.google.com/document/d/1ZubECnybyRJqfvB4mS4SRwlHiMvITVkHw9NchE0oXPw
BASE_FEATURE(kSurfaceEmbed, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

}  // namespace surface_embed
