// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_UTILS_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_UTILS_H_

#include <string>

#include "content/public/browser/render_frame_host.h"

namespace dom_distiller {

// Set the world for JavaScript to execute in. This can only be called once.
void SetDistillerJavaScriptWorldId(const int32_t id);

bool DistillerJavaScriptWorldIdIsSet();

// Execute JavaScript in an isolated world.
void RunIsolatedJavaScript(
    content::RenderFrameHost* render_frame_host,
    const std::string& buffer,
    content::RenderFrameHost::JavaScriptResultCallback callback);

// Same as above without a callback.
void RunIsolatedJavaScript(content::RenderFrameHost* render_frame_host,
                           const std::string& buffer);

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_UTILS_H_
