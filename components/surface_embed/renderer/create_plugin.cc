// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surface_embed/renderer/create_plugin.h"

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "components/surface_embed/common/constants.h"
#include "components/surface_embed/common/features.h"
#include "components/surface_embed/renderer/surface_embed_web_plugin.h"
#include "third_party/blink/public/web/web_plugin_params.h"

namespace surface_embed {

bool MaybeCreatePlugin(content::RenderFrame* render_frame,
                       const blink::WebPluginParams& params,
                       blink::WebPlugin** plugin) {
  if (!base::FeatureList::IsEnabled(features::kSurfaceEmbed)) {
    return false;
  }

  if (!base::EqualsCaseInsensitiveASCII(params.mime_type.Utf8(),
                                        kInternalPluginMimeType)) {
    return false;
  }

  *plugin = SurfaceEmbedWebPlugin::Create(render_frame, params);
  return true;
}

}  // namespace surface_embed
