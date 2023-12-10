// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_RENDERER_URL_REWRITE_RULES_PROVIDER_H_
#define COMPONENTS_CAST_RECEIVER_RENDERER_URL_REWRITE_RULES_PROVIDER_H_

#include "base/functional/callback.h"
#include "components/url_rewrite/renderer/url_request_rules_receiver.h"
#include "content/public/renderer/render_frame_observer.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace cast_receiver {

// This class provides URL request rewrite rules by binding a
// UrlRequestRulesReceiver mojo interface and listening for updates from
// browser. The lifespan of provider and rules is tied to a RenderFrame. Owned
// by CastRuntimeContentRendererClient, this object will be destroyed on
// RenderFrame destruction, triggering the destruction of all of the objects it
// exposes.
class UrlRewriteRulesProvider final : public content::RenderFrameObserver {
 public:
  // |on_render_frame_deleted_callback| must delete |this|.
  UrlRewriteRulesProvider(
      content::RenderFrame* render_frame,
      base::OnceCallback<void(const blink::LocalFrameToken&)>
          on_render_frame_deleted_callback);
  ~UrlRewriteRulesProvider() override;

  UrlRewriteRulesProvider(const UrlRewriteRulesProvider&) = delete;
  UrlRewriteRulesProvider& operator=(const UrlRewriteRulesProvider&) = delete;

  scoped_refptr<url_rewrite::UrlRequestRewriteRules> GetCachedRules() const;

 private:
  // content::RenderFrameObserver implementation.
  void OnDestruct() override;

  blink::LocalFrameToken frame_token_;
  url_rewrite::UrlRequestRulesReceiver url_request_rules_receiver_;
  base::OnceCallback<void(const blink::LocalFrameToken&)>
      on_render_frame_deleted_callback_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_RENDERER_URL_REWRITE_RULES_PROVIDER_H_
