// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_CONNECTIVITY_CHECKER_IMPL_H_
#define CHROMECAST_NET_CONNECTIVITY_CHECKER_IMPL_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/net/connectivity_checker.h"
#include "chromecast/net/time_sync_tracker.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace chromecast {

// Default (HTTPS) url for connectivity checking.
constexpr char kDefaultConnectivityCheckUrl[] =
    "https://connectivitycheck.gstatic.com/generate_204";

// HTTP url for connectivity checking.
constexpr char kHttpConnectivityCheckUrl[] =
    "http://connectivitycheck.gstatic.com/generate_204";

// The default URLs above are expected to respond with HTTP 204 (no content).
constexpr net::HttpStatusCode kConnectivitySuccessStatusCode =
    net::HTTP_NO_CONTENT;

// Delay notification of network change events to smooth out rapid flipping.
constexpr base::TimeDelta kNetworkChangedDelay = base::Seconds(3);

// Simple class to check network connectivity by sending a HEAD http request
// to given url.
class ConnectivityCheckerImpl
    : public ConnectivityChecker,
      public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public TimeSyncTracker::Observer {
 public:
  // Types of errors that can occur when making a network URL request.
  enum class ErrorType {
    BAD_HTTP_STATUS = 1,
    SSL_CERTIFICATE_ERROR = 2,
    REQUEST_TIMEOUT = 3,

    // A network error not captured by the ones above. See
    // net/base/net_error_list.h for full set of errors that might fall under
    // this category.
    NET_ERROR = 4,
  };

  // Connectivity checking and initialization will run on task_runner.
  static scoped_refptr<ConnectivityCheckerImpl> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      TimeSyncTracker* time_sync_tracker);

  // Connectivity checking and initialization will run on task_runner.
  static scoped_refptr<ConnectivityCheckerImpl> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      base::TimeDelta disconnected_probe_period,
      base::TimeDelta connected_probe_period,
      TimeSyncTracker* time_sync_tracker);

  ConnectivityCheckerImpl(const ConnectivityCheckerImpl&) = delete;
  ConnectivityCheckerImpl& operator=(const ConnectivityCheckerImpl&) = delete;

  // ConnectivityChecker implementation:
  bool Connected() const override;
  void Check() override;

  void SetCastMetricsHelperForTesting(
      metrics::CastMetricsHelper* cast_metrics_helper);

 protected:
  ConnectivityCheckerImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      network::NetworkConnectionTracker* network_connection_tracker,
      base::TimeDelta disconnected_probe_period,
      base::TimeDelta connected_probe_period,
      TimeSyncTracker* time_sync_tracker);
  ~ConnectivityCheckerImpl() override;

 private:
  // Initializes ConnectivityChecker
  void Initialize(std::unique_ptr<network::PendingSharedURLLoaderFactory>
                      pending_url_loader_factory);

  // network::NetworkConnectionTracker::NetworkConnectionObserver
  // implementation:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // TimeSyncTracker::Observer implementation:
  void OnTimeSynced() override;

  void OnConnectionChangedInternal();

  void OnConnectivityCheckComplete(
      scoped_refptr<net::HttpResponseHeaders> headers);

  // Cancels current connectivity checking in progress.
  void Cancel();

  // Sets connectivity and alerts observers if it has changed
  void SetConnected(bool connected);

  // Called when URL request failed.
  void OnUrlRequestError(ErrorType type);

  // Called when URL request timed out. |Timeout| stores how long we waited
  // for the URL request to finish before giving up.
  void OnUrlRequestTimeout(base::TimeDelta timeout);

  void CheckInternal();

  std::unique_ptr<GURL> connectivity_check_url_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  network::NetworkConnectionTracker* const network_connection_tracker_;
  TimeSyncTracker* const time_sync_tracker_;
  metrics::CastMetricsHelper* cast_metrics_helper_;

  // connected_lock_ protects access to connected_ which is shared across
  // threads.
  mutable base::Lock connected_lock_;
  // Represents that the device has network connectivity and that time has
  // synced.
  bool connected_and_time_synced_;

  // If the device has network connectivity.
  bool network_connected_;

  network::mojom::ConnectionType connection_type_;
  // Number of connectivity check errors.
  unsigned int check_errors_;
  bool network_changed_pending_;
  // Timeout handler for connectivity checks.
  // Note: Cancelling this timeout can cause the destructor for this class to be
  // called.
  base::CancelableOnceCallback<void()> timeout_;

  // Cancelable check handler used to cancel duplicate connectivity check.
  base::CancelableOnceCallback<void()> delayed_check_;

  // How often connectivity checks are performed while not connected.
  const base::TimeDelta disconnected_probe_period_;
  // How often connectivity checks are performed while connected.
  const base::TimeDelta connected_probe_period_;
  // Keeps track of whether this is the first time checking network
  // connectivity due to a network change. To prevent unnecessary delays in Cast
  // receiver initialization, kNetworkChangedDelay should only be applied on
  // network changes after the first one.
  bool first_connection_ = true;

  base::WeakPtr<ConnectivityCheckerImpl> weak_this_;
  base::WeakPtrFactory<ConnectivityCheckerImpl> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_CONNECTIVITY_CHECKER_IMPL_H_
