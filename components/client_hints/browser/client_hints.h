// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_H_
#define COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "ui/gfx/geometry/size_f.h"

class GURL;
class HostContentSettingsMap;
class PrefService;

namespace blink {
struct UserAgentMetadata;
class EnabledClientHints;
}  // namespace blink

namespace client_hints {

class ClientHints : public KeyedService,
                    public content::ClientHintsControllerDelegate {
 public:
  ClientHints(content::BrowserContext* context,
              network::NetworkQualityTracker* network_quality_tracker,
              HostContentSettingsMap* settings_map,
              scoped_refptr<content_settings::CookieSettings> cookie_settings,
              PrefService* pref_service);

  ClientHints(const ClientHints&) = delete;
  ClientHints& operator=(const ClientHints&) = delete;

  ~ClientHints() override;

  // content::ClientHintsControllerDelegate:
  network::NetworkQualityTracker* GetNetworkQualityTracker() override;

  void GetAllowedClientHintsFromSource(
      const url::Origin& origin,
      blink::EnabledClientHints* client_hints) override;

  bool IsJavaScriptAllowed(const GURL& url,
                           content::RenderFrameHost* parent_rfh) override;

  bool AreThirdPartyCookiesBlocked(const GURL& url,
                                   content::RenderFrameHost* rfh) override;

  blink::UserAgentMetadata GetUserAgentMetadata() override;

  void PersistClientHints(const url::Origin& primary_origin,
                          content::RenderFrameHost* parent_rfh,
                          const std::vector<network::mojom::WebClientHintsType>&
                              client_hints) override;

  void SetAdditionalClientHints(
      const std::vector<network::mojom::WebClientHintsType>&) override;

  void ClearAdditionalClientHints() override;

  void SetMostRecentMainFrameViewportSize(
      const gfx::Size& viewport_size) override;
  gfx::Size GetMostRecentMainFrameViewportSize() override;

  void ForceEmptyViewportSizeForTesting(
      bool should_force_empty_viewport_size) override;
  bool ShouldForceEmptyViewportSize() override;

 private:
  raw_ptr<content::BrowserContext> context_ = nullptr;
  raw_ptr<network::NetworkQualityTracker> network_quality_tracker_ = nullptr;
  raw_ptr<HostContentSettingsMap> settings_map_ = nullptr;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  std::vector<network::mojom::WebClientHintsType> additional_hints_;
  raw_ptr<PrefService> pref_service_;
  bool should_force_empty_viewport_size_{false};

  // This stores the viewport size of the most recent visible main frame tree
  // node. This value is only used when the viewport size cannot be directly
  // queried such as for prefetch requests and for tab restores.
  gfx::Size viewport_size_;
};

}  // namespace client_hints

#endif  // COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_H_
