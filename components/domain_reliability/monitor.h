// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_MONITOR_H_
#define COMPONENTS_DOMAIN_RELIABILITY_MONITOR_H_

#include <stddef.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/clear_mode.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/context.h"
#include "components/domain_reliability/context_manager.h"
#include "components/domain_reliability/dispatcher.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "components/domain_reliability/scheduler.h"
#include "components/domain_reliability/uploader.h"
#include "components/domain_reliability/util.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_error_details.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_response_info.h"
#include "net/socket/connection_attempts.h"
#include "net/url_request/url_request_status.h"

namespace base {
class Value;
}  // namespace base

namespace net {
class URLRequest;
class URLRequestContext;
class URLRequestContextGetter;
}  // namespace net

namespace domain_reliability {

// The top-level object that measures requests and hands off the measurements
// to the proper |DomainReliabilityContext|.
class DOMAIN_RELIABILITY_EXPORT DomainReliabilityMonitor
    : public net::NetworkChangeNotifier::NetworkChangeObserver,
      DomainReliabilityContext::Factory {
 public:
  // Creates a Monitor.
  DomainReliabilityMonitor(
      const std::string& upload_reporter_string,
      const DomainReliabilityContext::UploadAllowedCallback&
          upload_allowed_callback);

  // Same, but specifies a mock interface for time functions for testing.
  DomainReliabilityMonitor(
      const std::string& upload_reporter_string,
      const DomainReliabilityContext::UploadAllowedCallback&
          upload_allowed_callback,
      std::unique_ptr<MockableTime> time);

  ~DomainReliabilityMonitor() override;

  // Initializes the Monitor's URLRequestContextGetter.
  void InitURLRequestContext(net::URLRequestContext* url_request_context);

  // Same, but for unittests where the Getter is readily available.
  void InitURLRequestContext(
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter);

  // Shuts down the monitor prior to destruction. Currently, ensures that there
  // are no pending uploads, to avoid hairy lifetime issues at destruction.
  void Shutdown();

  // Populates the monitor with contexts that were configured at compile time.
  void AddBakedInConfigs();

  // Sets whether the uploader will discard uploads. Must be called after
  // |InitURLRequestContext|.
  void SetDiscardUploads(bool discard_uploads);

  // Should be called when |request| is about to follow a redirect. Will
  // examine and possibly log the redirect request. Must be called after
  // |SetDiscardUploads|.
  void OnBeforeRedirect(net::URLRequest* request);

  // Should be called when |request| is complete. Will examine and possibly
  // log the (final) request. |started| should be true if the request was
  // actually started before it was terminated. Must be called after
  // |SetDiscardUploads|.
  void OnCompleted(net::URLRequest* request, bool started);

  // net::NetworkChangeNotifier::NetworkChangeObserver implementation:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Called to remove browsing data for origins matched by |origin_filter|.
  // With CLEAR_BEACONS, leaves contexts in place but clears beacons (which
  // betray browsing history); with CLEAR_CONTEXTS, removes entire contexts
  // (which can behave as cookies). A null |origin_filter| is interpreted
  // as an always-true filter, indicating complete deletion.
  void ClearBrowsingData(
      DomainReliabilityClearMode mode,
      const base::Callback<bool(const GURL&)>& origin_filter);

  // Gets a Value containing data that can be formatted into a web page for
  // debugging purposes.
  std::unique_ptr<base::Value> GetWebUIData() const;

  DomainReliabilityContext* AddContextForTesting(
      std::unique_ptr<const DomainReliabilityConfig> config);

  size_t contexts_size_for_testing() const {
    return context_manager_.contexts_size_for_testing();
  }

  // Forces all pending uploads to run now, even if their minimum delay has not
  // yet passed.
  void ForceUploadsForTesting();

  // DomainReliabilityContext::Factory implementation:
  std::unique_ptr<DomainReliabilityContext> CreateContextForConfig(
      std::unique_ptr<const DomainReliabilityConfig> config) override;

 private:
  friend class DomainReliabilityMonitorTest;
  friend class DomainReliabilityServiceTest;
  // Allow the Service to call |MakeWeakPtr|.
  friend class DomainReliabilityServiceImpl;

  typedef std::map<std::string, DomainReliabilityContext*> ContextMap;

  struct DOMAIN_RELIABILITY_EXPORT RequestInfo {
    RequestInfo();
    explicit RequestInfo(const net::URLRequest& request);
    RequestInfo(const RequestInfo& other);
    ~RequestInfo();

    static bool ShouldReportRequest(const RequestInfo& request);

    GURL url;
    net::URLRequestStatus status;
    net::HttpResponseInfo response_info;
    int load_flags;
    net::LoadTimingInfo load_timing_info;
    net::ConnectionAttempts connection_attempts;
    net::IPEndPoint remote_endpoint;
    int upload_depth;
    net::NetErrorDetails details;
  };

  void OnRequestLegComplete(const RequestInfo& info);

  void MaybeHandleHeader(const RequestInfo& info);

  base::WeakPtr<DomainReliabilityMonitor> MakeWeakPtr();

  std::unique_ptr<MockableTime> time_;
  base::TimeTicks last_network_change_time_;
  const std::string upload_reporter_string_;
  DomainReliabilityContext::UploadAllowedCallback upload_allowed_callback_;
  DomainReliabilityScheduler::Params scheduler_params_;
  DomainReliabilityDispatcher dispatcher_;
  std::unique_ptr<DomainReliabilityUploader> uploader_;
  DomainReliabilityContextManager context_manager_;

  bool discard_uploads_set_;

  base::WeakPtrFactory<DomainReliabilityMonitor> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityMonitor);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_MONITOR_H_
