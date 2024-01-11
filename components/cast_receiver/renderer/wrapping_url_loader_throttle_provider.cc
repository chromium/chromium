// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/renderer/wrapping_url_loader_throttle_provider.h"

#include "components/cast_receiver/renderer/url_rewrite_rules_provider.h"
#include "components/media_control/renderer/media_playback_options.h"
#include "components/on_load_script_injector/renderer/on_load_script_injector.h"
#include "components/url_rewrite/common/url_loader_throttle.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace cast_receiver {

WrappingURLLoaderThrottleProvider::Client::~Client() = default;

WrappingURLLoaderThrottleProvider::WrappingURLLoaderThrottleProvider(
    std::unique_ptr<blink::URLLoaderThrottleProvider> wrapped_provider,
    Client& client)
    : client_(client), wrapped_provider_(std::move(wrapped_provider)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WrappingURLLoaderThrottleProvider::WrappingURLLoaderThrottleProvider(
    Client& client)
    : WrappingURLLoaderThrottleProvider(nullptr, client) {}

WrappingURLLoaderThrottleProvider::~WrappingURLLoaderThrottleProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
WrappingURLLoaderThrottleProvider::Clone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<WrappingURLLoaderThrottleProvider>(
      wrapped_provider_ ? wrapped_provider_->Clone() : nullptr, *client_);
}

blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>>
WrappingURLLoaderThrottleProvider::CreateThrottles(
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    const network::ResourceRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
  if (wrapped_provider_) {
    throttles = wrapped_provider_->CreateThrottles(local_frame_token, request);
  }

  if (!local_frame_token.has_value()) {
    return throttles;
  }
  auto* provider =
      client_->GetUrlRewriteRulesProvider(local_frame_token.value());
  if (provider) {
    auto rules = provider->GetCachedRules();
    if (rules) {
      throttles.emplace_back(std::make_unique<url_rewrite::URLLoaderThrottle>(
          rules,
          base::BindRepeating(
              &WrappingURLLoaderThrottleProvider::Client::IsCorsExemptHeader,
              base::Unretained(client_))));
    }
  }

  return throttles;
}

void WrappingURLLoaderThrottleProvider::SetOnline(bool is_online) {
  if (wrapped_provider_) {
    wrapped_provider_->SetOnline(is_online);
  }
}

}  // namespace cast_receiver
