// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/renderer/cast_runtime_url_loader_throttle_provider.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "chromecast/cast_core/runtime/common/cors_exempt_headers.h"
#include "chromecast/cast_core/runtime/renderer/cast_runtime_content_renderer_client.h"
#include "components/url_rewrite/common/url_loader_throttle.h"

namespace chromecast {

CastRuntimeURLLoaderThrottleProvider::CastRuntimeURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType type,
    CastRuntimeContentRendererClient* renderer_client)
    : type_(type), renderer_client_(renderer_client) {
  DCHECK(renderer_client_);
  DETACH_FROM_THREAD(thread_checker_);
}

CastRuntimeURLLoaderThrottleProvider::~CastRuntimeURLLoaderThrottleProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

CastRuntimeURLLoaderThrottleProvider::CastRuntimeURLLoaderThrottleProvider(
    const chromecast::CastRuntimeURLLoaderThrottleProvider& other)
    : type_(other.type_), renderer_client_(other.renderer_client_) {
  DETACH_FROM_THREAD(thread_checker_);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
CastRuntimeURLLoaderThrottleProvider::Clone() {
  return base::WrapUnique(new CastRuntimeURLLoaderThrottleProvider(*this));
}

blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>>
CastRuntimeURLLoaderThrottleProvider::CreateThrottles(
    int render_frame_id,
    const blink::WebURLRequest& request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;

  const auto& rules = renderer_client_->GetUrlRewriteRules(render_frame_id);
  if (rules) {
    auto url_loader_throttle = std::make_unique<url_rewrite::URLLoaderThrottle>(
        rules, base::BindRepeating(&IsHeaderCorsExempt));
    throttles.emplace_back(std::move(url_loader_throttle));
  }
  return throttles;
}

void CastRuntimeURLLoaderThrottleProvider::SetOnline(bool is_online) {}

}  // namespace chromecast
