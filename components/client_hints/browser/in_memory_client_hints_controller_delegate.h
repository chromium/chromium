// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLIENT_HINTS_BROWSER_IN_MEMORY_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
#define COMPONENTS_CLIENT_HINTS_BROWSER_IN_MEMORY_CLIENT_HINTS_CONTROLLER_DELEGATE_H_

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace blink {
class EnabledClientHints;
}

namespace network {
class NetworkQualityTracker;
}

namespace url {
class Origin;
}

namespace client_hints {

// In-memory manager of Accept-CH Cache used in Client Hints infrastructure.
// See https://wicg.github.io/client-hints-infrastructure/.
//
// This cache is not persisted and has the same lifetime as the delegate.
// Differs from client_hints::ClientHints by not relying on ContentSettings to
// store Accept-CH Cache.
class InMemoryClientHintsControllerDelegate final
    : public content::ClientHintsControllerDelegate {
 public:
  InMemoryClientHintsControllerDelegate(
      network::NetworkQualityTracker* network_quality_tracker,
      base::RepeatingCallback<bool(const GURL&)> is_javascript_allowed_callback,
      base::RepeatingCallback<bool(const GURL&)>
          are_third_party_cookies_blocked_callback,
      blink::UserAgentMetadata user_agent_metadata);
  ~InMemoryClientHintsControllerDelegate() override;

  InMemoryClientHintsControllerDelegate(
      InMemoryClientHintsControllerDelegate&) = delete;
  InMemoryClientHintsControllerDelegate& operator=(
      InMemoryClientHintsControllerDelegate&) = delete;

  // content::ClientHintsControllerDelegate implementation:
  void PersistClientHints(const url::Origin& primary_origin,
                          content::RenderFrameHost* parent_rfh,
                          const std::vector<network::mojom::WebClientHintsType>&
                              client_hints) override;
  void GetAllowedClientHintsFromSource(
      const url::Origin& origin,
      blink::EnabledClientHints* client_hints) override;
  void SetAdditionalClientHints(
      const std::vector<network::mojom::WebClientHintsType>&) override;
  void ClearAdditionalClientHints() override;
  network::NetworkQualityTracker* GetNetworkQualityTracker() override;
  bool IsJavaScriptAllowed(const GURL& url,
                           content::RenderFrameHost* parent_rfh) override;
  bool AreThirdPartyCookiesBlocked(const GURL& url,
                                   content::RenderFrameHost* rfh) override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  void SetMostRecentMainFrameViewportSize(
      const gfx::Size& viewport_size) override;
  gfx::Size GetMostRecentMainFrameViewportSize() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Stores enabled Client Hint types for an origin.
  std::map<url::Origin, blink::EnabledClientHints> accept_ch_cache_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Additional Client Hint types for Client Hints Reliability. If additional
  // hints are set, they would be included by subsequent calls to
  // GetAllowedClientHintsFromSource.
  std::vector<network::mojom::WebClientHintsType> additional_hints_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Tracker for networking-related hints. Must outlive this object.
  raw_ptr<network::NetworkQualityTracker> network_quality_tracker_;

  // Callback to determine whether JavaScript is enabled for an URL.
  base::RepeatingCallback<bool(const GURL&)> is_javascript_allowed_callback_;

  // Callback to determine whether third-party cookies are blocked for an URL.
  base::RepeatingCallback<bool(const GURL&)>
      are_third_party_cookies_blocked_callback_;

  const blink::UserAgentMetadata user_agent_metadata_;

  // This stores the viewport size of the most recent visible main frame tree
  // node. This value is only used when the viewport size cannot be directly
  // queried such as for prefetch requests and for tab restores.
  gfx::Size viewport_size_;
};

}  // namespace client_hints

#endif  // COMPONENTS_CLIENT_HINTS_BROWSER_IN_MEMORY_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
