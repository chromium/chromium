// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_UTILS_H_
#define COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_UTILS_H_

#include "base/component_export.h"

namespace content {
class WebContents;
}  // namespace content

namespace secure_embed {

COMPONENT_EXPORT(SECURE_EMBED)
bool IsSecureEmbedGuestWebContents(content::WebContents* web_contents);

}  // namespace secure_embed

#endif  // COMPONENTS_SURFACE_BROWSER_SECURE_EMBED_UTILS_H_
