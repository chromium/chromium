// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_MONITOR_H_
#define COMPONENTS_DOMAIN_RELIABILITY_MONITOR_H_

#include <stddef.h>

#include <map>
#include <memory>

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
#include "net/base/isolation_info.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_error_details.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_response_info.h"
#include "net/socket/connection_attempts.h"

namespace net {
class URLRequest;
class URLRequestContext;
}  // namespace net

namespace domain_reliability {

// The top-level object that measures requests and hands off the measurements
// to the proper |DomainReliabilityContext|.
// To initialize this object fully, you need to call AddBakedInConfigs() and
// SetDiscardUploads() before using this.
class DOMAIN_RELIABILITY_EXPORT DomainReliabilityMonitor
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  struct DOMAIN_RELIABILITY_EXPORT RequestInfo {
    RequestInfo();
    RequestInfo(const net::URLRequest& request, int net_error);
    RequestInfo(const RequestInfo& other);
    ~RequestInfo();

    static bool ShouldReportRequest(const RequestInfo& request);

    GURL url;
    net::IsolationInfo isolation_info;
    int net_error;
    net::HttpResponseInfo response_info;
    bool allow_credentials;
    net::LoadTimingInfo load_timing_info;
    net::ConnectionAttempts connection_attempts;
    net::IPEndPoint remote_endpoint;
    int upload_depth;
    net::NetErrorDetails details;
  };

  // Creates a Monitor.
  DomainReliabilityMonitor(
      net::URLRequestContext* url_request_context,
      const std::string& upload_reporter_string,
      const DomainReliabilityContext::UploadAllowedCallback&
          upload_allowed_callback);

  // Same, but specifies a mock interface for time functions for testing.
  DomainReliabilityMonitor(
      net::URLRequestContext* url_request_context,
      const std::string& upload_reporter_string,
      const DomainReliabilityContext::UploadAllowedCallback&
          upload_allowed_callback,
      std::unique_ptr<MockableTime> time);

  DomainReliabilityMonitor(const DomainReliabilityMonitor&) = delete;
  DomainReliabilityMonitor& operator=(const DomainReliabilityMonitor&) = delete;

  ~DomainReliabilityMonitor() override;

  // Shuts down the monitor prior to destruction. Currently, ensures that there
  // are no pending uploads, to avoid hairy lifetime issues at destruction.
  void Shutdown();

  // Populates the monitor with contexts that were configured at compile time.
  // Note: This is separate from the Google configs, which are created
  // on demand at runtime.
  void AddBakedInConfigs();

  // Sets whether the uploader will discard uploads.
  void SetDiscardUploads(bool discard_uploads);

  // Should be called when |request| is about to follow a redirect. Will
  // examine and possibly log the redirect request. Must be called after
  // |SetDiscardUploads|.
  void OnBeforeRedirect(net::URLRequest* request);

  // Should be called when |request| is complete. Will examine and possibly
  // log the (final) request. |started| should be true if the request was
  // actually started before it was terminated. |net_error| should be the
  // final result of the network request. Must be called after
  // |SetDiscardUploads|.
  void OnCompleted(net::URLRequest* request, bool started, int net_error);

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
      const base::RepeatingCallback<bool(const url::Origin&)>& origin_filter);

  // Returns pointer to the added context.
  const DomainReliabilityContext* AddContextForTesting(
      std::unique_ptr<const DomainReliabilityConfig> config);

  size_t contexts_size_for_testing() const {
    return context_manager_.contexts_size_for_testing();
  }

  // Forces all pending uploads to run now, even if their minimum delay has not
  // yet passed.
  void ForceUploadsForTesting();

  void OnRequestLegCompleteForTesting(const RequestInfo& info);

  const DomainReliabilityContext* LookupContextForTesting(
      const std::string& hostname) const;

 private:
  void OnRequestLegComplete(const RequestInfo& info);

  std::unique_ptr<MockableTime> time_;
  DomainReliabilityScheduler::Params scheduler_params_;
  DomainReliabilityDispatcher dispatcher_;
  std::unique_ptr<DomainReliabilityUploader> uploader_;
  DomainReliabilityContextManager context_manager_;

  bool discard_uploads_set_;
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_MONITOR_H_
