// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_URL_LIST_POLICY_BLOCKLIST_SERVICE_H_
#define COMPONENTS_POLICY_CORE_BROWSER_URL_LIST_POLICY_BLOCKLIST_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/policy/policy_export.h"

// PolicyBlocklistService provides a way for us to access URLBlocklistManager,
// a policy block list service based on the Preference Service. The
// URLBlocklistManager responds to permission changes and is per-Profile.
// An instance of this KeyedService is created and managed by an
// embedder-specific factory. For example, ChromePolicyBlocklistServiceFactory
// is used in Chrome.
class POLICY_EXPORT PolicyBlocklistService : public KeyedService {
 public:
  PolicyBlocklistService(
      std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager,
      PrefService* user_prefs);

  PolicyBlocklistService(const PolicyBlocklistService&) = delete;
  PolicyBlocklistService& operator=(const PolicyBlocklistService&) = delete;
  ~PolicyBlocklistService() override;

  policy::URLBlocklist::URLBlocklistState GetURLBlocklistState(
      const GURL& url) const;

#if BUILDFLAG(IS_CHROMEOS)
  // Configures the URL filters source the `url_blocklist_manager_`. If
  // `enforced` is false, the default URL filters source is used (i.e. the
  // URLBlocklist and URLAllowlist prefs). If `enforced` is true, the
  // `url_blocklist_manager_` is configured to use a custom source for URL
  // filters.
  void SetAlwaysOnVpnPreConnectUrlAllowlistEnforced(bool enforced);
#endif

 private:
  std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager_;
  raw_ptr<PrefService> user_prefs_;
};

#endif  // COMPONENTS_POLICY_CORE_BROWSER_URL_LIST_POLICY_BLOCKLIST_SERVICE_H_
