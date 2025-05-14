// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_EVENT_ROUTER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace enterprise_connectors {

// An event router that collects safe browsing events and then sends
// events to reporting server.
class ReportingEventRouter : public KeyedService {
  using ReferrerChain =
      google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>;

 public:
  explicit ReportingEventRouter(RealtimeReportingClientBase* reporting_client);

  ReportingEventRouter(const ReportingEventRouter&) = delete;
  ReportingEventRouter& operator=(const ReportingEventRouter&) = delete;
  ReportingEventRouter(ReportingEventRouter&&) = delete;
  ReportingEventRouter& operator=(ReportingEventRouter&&) = delete;

  ~ReportingEventRouter() override;

  bool IsEventEnabled(const std::string& event);

  virtual void OnLoginEvent(const GURL& url,
                            bool is_federated,
                            const url::SchemeHostPort& federated_origin,
                            const std::u16string& username);

  virtual void OnPasswordBreach(
      const std::string& trigger,
      const std::vector<std::pair<GURL, std::u16string>>& identities);

  // Notifies listeners that the user reused a protected password.
  // - `url` is the URL where the password was reused
  // - `user_name` is the user associated with the reused password
  // - `is_phising_url` is whether the URL is thought to be a phishing one
  // - `warning_shown` is whether a warning dialog was shown to the user
  void OnPasswordReuse(const GURL& url,
                       const std::string& user_name,
                       bool is_phishing_url,
                       bool warning_shown);

  // Notifies listeners that the user changed the password associated with
  // `user_name`
  void OnPasswordChanged(const std::string& user_name);

  // Notifies listeners about events related to Url Filtering Interstitials.
  // Virtual for tests.
  virtual void OnUrlFilteringInterstitial(
      const GURL& url,
      const std::string& threat_type,
      const safe_browsing::RTLookupResponse& response,
      const ReferrerChain& referrer_chain);

  // Notifies listeners that the user clicked-through a security interstitial.
  void OnSecurityInterstitialProceeded(const GURL& url,
                                       const std::string& reason,
                                       int net_error_code,
                                       const ReferrerChain& referrer_chain);

  // Notifies listeners that the user saw a security interstitial.
  void OnSecurityInterstitialShown(const GURL& url,
                                   const std::string& reason,
                                   int net_error_code,
                                   bool proceed_anyway_disabled,
                                   const ReferrerChain& referrer_chain);

 private:
  raw_ptr<RealtimeReportingClientBase> reporting_client_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_EVENT_ROUTER_H_
