// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_WELL_KNOWN_CHANGE_PASSWORD_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_PASSWORDS_WELL_KNOWN_CHANGE_PASSWORD_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/well_known_change_password/well_known_change_password_state.h"
#include "components/password_manager/core/browser/well_known_change_password/well_known_change_password_util.h"
#include "content/public/browser/navigation_throttle.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace affiliations {
class AffiliationService;
}  // namespace affiliations

namespace content {
class NavigationHandle;
}  // namespace content

// This NavigationThrottle checks whether a site supports the
// .well-known/change-password url. To check whether a site supports the
// change-password url, we also request a .well-known path that is defined to
// return a 404. When that one returns a 404 and the change password path a 200
// we assume the site supports the change-password url. If the site does not
// support the change password url, the user gets redirected to the base path
// '/'.
class WellKnownChangePasswordNavigationThrottle
    : public content::NavigationThrottle,
      public password_manager::WellKnownChangePasswordStateDelegate {
 public:
  static std::unique_ptr<WellKnownChangePasswordNavigationThrottle>
  MaybeCreateThrottleFor(content::NavigationHandle* handle);

  explicit WellKnownChangePasswordNavigationThrottle(
      content::NavigationHandle* handle);

  ~WellKnownChangePasswordNavigationThrottle() override;

  // We don't need to override WillRedirectRequest since a redirect is the
  // expected behaviour and does not need manual intervention.
  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillFailRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  // password_manager::WellKnownChangePasswordStateDelegate:
  void OnProcessingFinished(bool is_supported) override;
  // Redirects to a given URL in the same tab.
  void Redirect(const GURL& url);
  // Records the given UKM metric.
  void RecordMetric(password_manager::WellKnownChangePasswordResult result);

  // Stores `navigation_handle()->GetURL()` if the first navigation was to
  // .well-known/change-password. It is later used to derive the URL for the
  // non-existing resource, and to provide fallback logic.
  const GURL request_url_;
  password_manager::WellKnownChangePasswordState
      well_known_change_password_state_{this};
  ukm::SourceId source_id_ = ukm::kInvalidSourceId;
  raw_ptr<affiliations::AffiliationService> affiliation_service_ = nullptr;
  base::WeakPtrFactory<password_manager::WellKnownChangePasswordState>
      weak_ptr_factory_{&well_known_change_password_state_};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_WELL_KNOWN_CHANGE_PASSWORD_NAVIGATION_THROTTLE_H_
