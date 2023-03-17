// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/client_hints/browser/in_memory_client_hints_controller_delegate.h"

#include <vector>

#include "base/functional/bind.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "url/origin.h"

namespace client_hints {

InMemoryClientHintsControllerDelegate::InMemoryClientHintsControllerDelegate(
    network::NetworkQualityTracker* network_quality_tracker,
    base::RepeatingCallback<bool(const GURL&)> is_javascript_allowed_callback,
    base::RepeatingCallback<bool(const GURL&)>
        are_third_party_cookies_blocked_callback,
    blink::UserAgentMetadata user_agent_metadata)
    : network_quality_tracker_(network_quality_tracker),
      is_javascript_allowed_callback_(is_javascript_allowed_callback),
      are_third_party_cookies_blocked_callback_(
          are_third_party_cookies_blocked_callback),
      user_agent_metadata_(user_agent_metadata) {}

InMemoryClientHintsControllerDelegate::
    ~InMemoryClientHintsControllerDelegate() = default;

// Enabled Client Hints are only cached and not persisted in this
// implementation.
void InMemoryClientHintsControllerDelegate::PersistClientHints(
    const url::Origin& primary_origin,
    content::RenderFrameHost* parent_rfh,
    const std::vector<network::mojom::WebClientHintsType>& client_hints) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const GURL primary_url = primary_origin.GetURL();
  DCHECK(primary_url.is_valid());
  DCHECK(network::IsUrlPotentiallyTrustworthy(primary_url));

  // Client hints should only be enabled when JavaScript is enabled.
  if (!IsJavaScriptAllowed(primary_url, parent_rfh))
    return;

  blink::EnabledClientHints enabled_hints;
  for (auto hint : client_hints) {
    enabled_hints.SetIsEnabled(hint, true);
  }
  accept_ch_cache_[primary_origin] = enabled_hints;
}

// Looks up enabled Client Hints for the URL origin, and adds additional Client
// Hints if set.
void InMemoryClientHintsControllerDelegate::GetAllowedClientHintsFromSource(
    const url::Origin& origin,
    blink::EnabledClientHints* client_hints) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_hints);
  if (!network::IsOriginPotentiallyTrustworthy(origin)) {
    return;
  }

  const auto& it = accept_ch_cache_.find(origin);
  if (it != accept_ch_cache_.end()) {
    *client_hints = it->second;
  }

  for (auto hint : additional_hints_)
    client_hints->SetIsEnabled(hint, true);
}

void InMemoryClientHintsControllerDelegate::SetAdditionalClientHints(
    const std::vector<network::mojom::WebClientHintsType>& hints) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  additional_hints_ = hints;
}

void InMemoryClientHintsControllerDelegate::ClearAdditionalClientHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  additional_hints_.clear();
}

network::NetworkQualityTracker*
InMemoryClientHintsControllerDelegate::GetNetworkQualityTracker() {
  return network_quality_tracker_;
}

bool InMemoryClientHintsControllerDelegate::IsJavaScriptAllowed(
    const GURL& url,
    content::RenderFrameHost* parent_rfh) {
  return is_javascript_allowed_callback_.Run(url);
}

bool InMemoryClientHintsControllerDelegate::AreThirdPartyCookiesBlocked(
    const GURL& url,
    content::RenderFrameHost* rfh) {
  return are_third_party_cookies_blocked_callback_.Run(url);
}

blink::UserAgentMetadata
InMemoryClientHintsControllerDelegate::GetUserAgentMetadata() {
  return user_agent_metadata_;
}

void InMemoryClientHintsControllerDelegate::SetMostRecentMainFrameViewportSize(
    const gfx::Size& viewport_size) {
  viewport_size_ = viewport_size;
}

gfx::Size
InMemoryClientHintsControllerDelegate::GetMostRecentMainFrameViewportSize() {
  return viewport_size_;
}

}  // namespace client_hints
