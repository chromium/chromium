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
 public:
  explicit ReportingEventRouter(RealtimeReportingClientBase* reporting_client);

  ReportingEventRouter(const ReportingEventRouter&) = delete;
  ReportingEventRouter& operator=(const ReportingEventRouter&) = delete;
  ReportingEventRouter(ReportingEventRouter&&) = delete;
  ReportingEventRouter& operator=(ReportingEventRouter&&) = delete;

  ~ReportingEventRouter() override;

  bool IsEventEnabled(const std::string& event);

  void OnLoginEvent(const GURL& url,
                    bool is_federated,
                    const url::SchemeHostPort& federated_origin,
                    const std::u16string& username);

  void OnPasswordBreach(
      const std::string& trigger,
      const std::vector<std::pair<GURL, std::u16string>>& identities);

  void OnUrlFilteringInterstitial(
      const GURL& url,
      const std::string& threat_type,
      const safe_browsing::RTLookupResponse& response);

 private:
  raw_ptr<RealtimeReportingClientBase> reporting_client_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_EVENT_ROUTER_H_
