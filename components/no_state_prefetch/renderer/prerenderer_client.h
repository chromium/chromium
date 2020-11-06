// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_RENDERER_PRERENDERER_CLIENT_H_
#define COMPONENTS_NO_STATE_PREFETCH_RENDERER_PRERENDERER_CLIENT_H_

#include "base/compiler_specific.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/blink/public/web/web_prerenderer_client.h"

namespace prerender {

class PrerendererClient : public content::RenderViewObserver,
                          public blink::WebPrerendererClient {
 public:
  explicit PrerendererClient(content::RenderView* render_view);

 private:
  ~PrerendererClient() override;

  // RenderViewObserver implementation.
  void OnDestruct() override;

  // Implements blink::WebPrerendererClient
  bool IsPrefetchOnly() override;
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_RENDERER_PRERENDERER_CLIENT_H_
