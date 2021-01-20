// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_H_
#define COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

class GURL;
class HostContentSettingsMap;

namespace client_hints {

class ClientHints : public KeyedService,
                    public content::ClientHintsControllerDelegate {
 public:
  ClientHints(content::BrowserContext* context,
              network::NetworkQualityTracker* network_quality_tracker,
              HostContentSettingsMap* settings_map,
              const blink::UserAgentMetadata& user_agent_metadata,
              PrefService* pref_service);
  ~ClientHints() override;

  // content::ClientHintsControllerDelegate:
  network::NetworkQualityTracker* GetNetworkQualityTracker() override;

  void GetAllowedClientHintsFromSource(
      const GURL& url,
      blink::WebEnabledClientHints* client_hints) override;

  bool IsJavaScriptAllowed(const GURL& url) override;

  bool UserAgentClientHintEnabled() override;

  blink::UserAgentMetadata GetUserAgentMetadata() override;

  void PersistClientHints(
      const url::Origin& primary_origin,
      const std::vector<network::mojom::WebClientHintsType>& client_hints,
      base::TimeDelta expiration_duration) override;

 private:
  content::BrowserContext* context_ = nullptr;
  network::NetworkQualityTracker* network_quality_tracker_ = nullptr;
  HostContentSettingsMap* settings_map_ = nullptr;
  blink::UserAgentMetadata user_agent_metadata_;
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(ClientHints);
};

}  // namespace client_hints

#endif  // COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_H_
