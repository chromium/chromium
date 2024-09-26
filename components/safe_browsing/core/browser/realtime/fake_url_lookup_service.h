// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_FAKE_URL_LOOKUP_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_FAKE_URL_LOOKUP_SERVICE_H_

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace safe_browsing::testing {

// A fake implementation of RealTimeUrlLookupServiceBase for use in tests.
// The `StartLookup()` virtual method is not implemented making this an
// abstract base class.  Most tests will need to derive from this class and
// only implement this method.
class FakeRealTimeUrlLookupService
    : public safe_browsing::RealTimeUrlLookupServiceBase {
 public:
  FakeRealTimeUrlLookupService();
  ~FakeRealTimeUrlLookupService() override = default;

  // RealTimeUrlLookupServiceBase:
  bool CanPerformFullURLLookup() const override;
  bool CanIncludeSubframeUrlInReferrerChain() const override;
  bool CanCheckSafeBrowsingDb() const override;
  bool CanCheckSafeBrowsingHighConfidenceAllowlist() const override;
  bool CanSendRTSampleRequest() const override;
  std::string GetUserEmail() const override;
  std::string GetBrowserDMTokenString() const override;
  std::string GetProfileDMTokenString() const override;
  std::unique_ptr<enterprise_connectors::ClientMetadata> GetClientMetadata()
      const override;
  std::string GetMetricSuffix() const override;
  void SendSampledRequest(
      const GURL& url,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID session_id) override;
  bool CanCheckUrl(const GURL& url) override;

  GURL GetRealTimeLookupUrl() const override;
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const override;
  bool CanPerformFullURLLookupWithToken() const override;
  int GetReferrerUserGestureLimit() const override;
  bool CanSendPageLoadToken() const override;
  void GetAccessToken(
      const GURL& url,
      safe_browsing::RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID session_id) override;
  std::optional<std::string> GetDMTokenString() const override;
  bool ShouldIncludeCredentials() const override;
  std::optional<base::Time> GetMinAllowedTimestampForReferrerChains()
      const override;
};

}  // namespace safe_browsing::testing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_FAKE_URL_LOOKUP_SERVICE_H_
