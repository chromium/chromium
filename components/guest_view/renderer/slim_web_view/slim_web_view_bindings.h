// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_RENDERER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_BINDINGS_H_
#define COMPONENTS_GUEST_VIEW_RENDERER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_BINDINGS_H_

#include "components/guest_view/common/guest_view.mojom-forward.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace guest_view {

class SlimWebViewBindings {
 public:
  static void MaybeInstall(content::RenderFrame& render_frame);
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_RENDERER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_BINDINGS_H_
