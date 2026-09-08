// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_CONTROLLER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/browser/reporting/pref_url_list_matcher.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace enterprise_reporting {

class SaasUsageReportingController : public KeyedService {
 public:
  // Delegate class that is used to collect navigation information.
  class NavigationDataDelegate {
   public:
    using EncryptionProtocolCallback =
        base::OnceCallback<void(std::string_view)>;

    virtual ~NavigationDataDelegate() = default;
    virtual GURL GetUrl() const = 0;
    virtual void GetEncryptionProtocol(
        EncryptionProtocolCallback callback) const = 0;
  };

  SaasUsageReportingController(
      PrefService* local_state_pref_service,
      PrefService* profile_pref_service,
      std::unique_ptr<PrefURLListMatcher> browser_matcher,
      std::unique_ptr<PrefURLListMatcher> profile_matcher);
  ~SaasUsageReportingController() override;

  virtual void RecordNavigation(const NavigationDataDelegate& delegate) const;

  // Records a single usage of a "Gemini in Chrome" feature.
  // This method is intended for tracking usage of Gemini integrated directly
  // into the browser, not the Gemini web application.
  virtual void RecordGeminiInChromeUsage() const;

 private:
  struct DomainMatches {
    std::optional<std::string> browser_domain;
    std::optional<std::string> profile_domain;

    bool HasMatch() const { return browser_domain || profile_domain; }
  };

  void OnEncryptionProtocolFetched(DomainMatches domains,
                                   std::string_view encryption_protocol) const;

  DomainMatches MatchDomains(const GURL& url) const;

  const raw_ref<PrefService> local_state_pref_service_;
  const raw_ref<PrefService> profile_pref_service_;
  std::unique_ptr<PrefURLListMatcher> browser_matcher_;
  std::unique_ptr<PrefURLListMatcher> profile_matcher_;

  mutable base::WeakPtrFactory<SaasUsageReportingController> weak_ptr_factory_{
      this};
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_CONTROLLER_H_
