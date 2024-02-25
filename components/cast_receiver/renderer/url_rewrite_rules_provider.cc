// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/renderer/url_rewrite_rules_provider.h"

#include "base/functional/bind.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace cast_receiver {

UrlRewriteRulesProvider::UrlRewriteRulesProvider(
    content::RenderFrame* render_frame,
    base::OnceCallback<void(const blink::LocalFrameToken&)>
        on_render_frame_deleted_callback)
    : content::RenderFrameObserver(render_frame),
      frame_token_(render_frame->GetWebFrame()->GetLocalFrameToken()),
      url_request_rules_receiver_(render_frame),
      on_render_frame_deleted_callback_(
          std::move(on_render_frame_deleted_callback)) {
  DCHECK(render_frame);
  DCHECK(on_render_frame_deleted_callback_);
}

UrlRewriteRulesProvider::~UrlRewriteRulesProvider() = default;

scoped_refptr<url_rewrite::UrlRequestRewriteRules>
UrlRewriteRulesProvider::GetCachedRules() const {
  return url_request_rules_receiver_.GetCachedRules();
}

void UrlRewriteRulesProvider::OnDestruct() {
  std::move(on_render_frame_deleted_callback_).Run(frame_token_);
}

}  // namespace cast_receiver
