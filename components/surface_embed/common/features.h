// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACE_EMBED_COMMON_FEATURES_H_
#define COMPONENTS_SURFACE_EMBED_COMMON_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace surface_embed {

namespace features {

// Enable the SurfaceEmbed mechanism for embedding WebContents via a WebPlugin.
// https://docs.google.com/document/d/1ZubECnybyRJqfvB4mS4SRwlHiMvITVkHw9NchE0oXPw
COMPONENT_EXPORT(SURFACE_EMBED_COMMON) BASE_DECLARE_FEATURE(kSurfaceEmbed);

}  // namespace features

}  // namespace surface_embed

#endif  // COMPONENTS_SURFACE_EMBED_COMMON_FEATURES_H_
