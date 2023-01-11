// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/isolated_world_ids.h"

namespace dom_distiller {

namespace {
// An invalid world ID to check against.
const int32_t invalid_world_id = -1;
// The ID of the world javascript should execute in; init to invalid ID.
int32_t distiller_javascript_world_id = invalid_world_id;
}  // namespace

void SetDistillerJavaScriptWorldId(const int32_t id) {
  // Never allow running in main world (0).
  DCHECK(id > content::ISOLATED_WORLD_ID_GLOBAL);
  // Only allow ID to be set once.
  DCHECK(distiller_javascript_world_id == invalid_world_id);
  distiller_javascript_world_id = id;
}

bool DistillerJavaScriptWorldIdIsSet() {
  return distiller_javascript_world_id != invalid_world_id;
}

void RunIsolatedJavaScript(
    content::RenderFrameHost* render_frame_host,
    const std::string& buffer,
    content::RenderFrameHost::JavaScriptResultCallback callback) {
  // Make sure world ID was set.
  DCHECK(distiller_javascript_world_id != invalid_world_id);
  render_frame_host->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(buffer), std::move(callback),
      distiller_javascript_world_id);
}

void RunIsolatedJavaScript(content::RenderFrameHost* render_frame_host,
                           const std::string& buffer) {
  RunIsolatedJavaScript(render_frame_host, buffer, {});
}

}  // namespace dom_distiller
