// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_CONTROLLER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_CONTROLLER_H_

#include <memory>
#include <string_view>

#include "base/memory/raw_ref.h"
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
    virtual ~NavigationDataDelegate() = default;
    virtual GURL GetUrl() const = 0;
    virtual std::string GetEncryptionProtocol() const = 0;
  };

  SaasUsageReportingController(
      PrefService* local_state_pref_service,
      PrefService* profile_pref_service,
      std::unique_ptr<PrefURLListMatcher> browser_matcher,
      std::unique_ptr<PrefURLListMatcher> profile_matcher);
  ~SaasUsageReportingController() override;

  virtual void RecordNavigation(const NavigationDataDelegate& delegate) const;

 private:
  const raw_ref<PrefService> local_state_pref_service_;
  const raw_ref<PrefService> profile_pref_service_;
  std::unique_ptr<PrefURLListMatcher> browser_matcher_;
  std::unique_ptr<PrefURLListMatcher> profile_matcher_;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_CONTROLLER_H_
