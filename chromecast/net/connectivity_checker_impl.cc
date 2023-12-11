// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/connectivity_checker_impl.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/net/net_switches.h"
#include "chromecast/net/time_sync_tracker.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace chromecast {

namespace {

// How often connectivity checks are performed while not connected.
constexpr base::TimeDelta kDisconnectedProbePeriod = base::Seconds(1);

// How often connectivity checks are performed while connected.
constexpr base::TimeDelta kConnectedProbePeriod = base::Seconds(60);

// Number of consecutive connectivity check errors before status is changed
// to offline.
const unsigned int kNumErrorsToNotifyOffline = 3;

// Request timeout value.
constexpr base::TimeDelta kRequestTimeout = base::Seconds(3);

const char kMetricNameNetworkConnectivityCheckingErrorType[] =
    "Network.ConnectivityChecking.ErrorType";

}  // namespace

// static
scoped_refptr<ConnectivityCheckerImpl> ConnectivityCheckerImpl::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    TimeSyncTracker* time_sync_tracker) {
  DCHECK(task_runner);

  return Create(task_runner, std::move(pending_url_loader_factory),
                network_connection_tracker, kDisconnectedProbePeriod,
                kConnectedProbePeriod, time_sync_tracker);
}

// static
scoped_refptr<ConnectivityCheckerImpl> ConnectivityCheckerImpl::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    base::TimeDelta disconnected_probe_period,
    base::TimeDelta connected_probe_period,
    TimeSyncTracker* time_sync_tracker) {
  DCHECK(task_runner);

  auto connectivity_checker = base::WrapRefCounted(new ConnectivityCheckerImpl(
      task_runner, network_connection_tracker, disconnected_probe_period,
      connected_probe_period, time_sync_tracker));
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&ConnectivityCheckerImpl::Initialize, connectivity_checker,
                     std::move(pending_url_loader_factory)));
  return connectivity_checker;
}

ConnectivityCheckerImpl::ConnectivityCheckerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    network::NetworkConnectionTracker* network_connection_tracker,
    base::TimeDelta disconnected_probe_period,
    base::TimeDelta connected_probe_period,
    TimeSyncTracker* time_sync_tracker)
    : ConnectivityChecker(task_runner),
      task_runner_(std::move(task_runner)),
      network_connection_tracker_(network_connection_tracker),
      time_sync_tracker_(time_sync_tracker),
      cast_metrics_helper_(metrics::CastMetricsHelper::GetInstance()),
      connected_and_time_synced_(false),
      network_connected_(false),
      connection_type_(network::mojom::ConnectionType::CONNECTION_NONE),
      check_errors_(0),
      network_changed_pending_(false),
      disconnected_probe_period_(disconnected_probe_period),
      connected_probe_period_(connected_probe_period),
      weak_factory_(this) {
  DCHECK(task_runner_);
  DCHECK(network_connection_tracker_);
  DCHECK(cast_metrics_helper_);
  DCHECK(!disconnected_probe_period_.is_zero());
  DCHECK(!connected_probe_period_.is_zero());
  weak_this_ = weak_factory_.GetWeakPtr();

  if (time_sync_tracker_) {
    time_sync_tracker_->AddObserver(this);
  }
}

void ConnectivityCheckerImpl::Initialize(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(pending_url_loader_factory);
  url_loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::move(pending_url_loader_factory));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringType check_url_str =
      command_line->GetSwitchValueNative(switches::kConnectivityCheckUrl);
  connectivity_check_url_ = std::make_unique<GURL>(
      check_url_str.empty() ? kDefaultConnectivityCheckUrl : check_url_str);

  network_connection_tracker_->AddNetworkConnectionObserver(this);

  Check();
}

ConnectivityCheckerImpl::~ConnectivityCheckerImpl() {
  DCHECK(task_runner_);
  DCHECK(task_runner_->BelongsToCurrentThread());
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

bool ConnectivityCheckerImpl::Connected() const {
  base::AutoLock auto_lock(connected_lock_);
  return connected_and_time_synced_;
}

void ConnectivityCheckerImpl::SetConnected(bool connected) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock auto_lock(connected_lock_);
    network_connected_ = connected;

    // If a time_sync_tracker is not provided, is it assumed that network
    // connectivity is equivalent to time being synced.
    bool connected_and_time_synced = network_connected_;
    if (time_sync_tracker_) {
      connected_and_time_synced &= time_sync_tracker_->IsTimeSynced();
    }

    if (connected_and_time_synced_ == connected_and_time_synced) {
      return;
    }

    connected_and_time_synced_ = connected_and_time_synced;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringType check_url_str =
      command_line->GetSwitchValueNative(switches::kConnectivityCheckUrl);
  if (check_url_str.empty()) {
    connectivity_check_url_ = std::make_unique<GURL>(
        connected_and_time_synced_ ? kHttpConnectivityCheckUrl
                                   : kDefaultConnectivityCheckUrl);
    LOG(INFO) << "Change check url=" << *connectivity_check_url_;
  }

  Notify(connected_and_time_synced_);
  LOG(INFO) << "Global connection is: "
            << (connected_and_time_synced_ ? "Up" : "Down");
}

void ConnectivityCheckerImpl::Check() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ConnectivityCheckerImpl::CheckInternal, weak_this_));
}

void ConnectivityCheckerImpl::CheckInternal() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(url_loader_factory_);

  auto connection_type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  bool is_sync = network_connection_tracker_->GetConnectionType(
      &connection_type,
      base::BindOnce(&ConnectivityCheckerImpl::OnConnectionChanged,
                     weak_this_));

  // Don't check connectivity if network is offline.
  // Also don't check connectivity if the connection_type cannot be
  // synchronously retrieved, since OnConnectionChanged will be triggered later
  // which will cause duplicate checks.
  if (!is_sync ||
      connection_type == network::mojom::ConnectionType::CONNECTION_NONE) {
    return;
  }

  // If url_loader_ is non-null, there is already a check going on. Don't
  // start another.
  if (url_loader_) {
    return;
  }

  VLOG(1) << "Connectivity check: url=" << *connectivity_check_url_;
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(*connectivity_check_url_);
  resource_request->method = "HEAD";
  resource_request->priority = net::MAXIMUM_PRIORITY;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 MISSING_TRAFFIC_ANNOTATION);

  // To enable ssl_info in the response.
  url_loader_->SetURLLoaderFactoryOptions(
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
      network::mojom::kURLLoadOptionSendSSLInfoForCertificateError);

  // Configure the loader to treat HTTP error status codes as successful loads.
  // This setting allows us to inspect the status code and log it as an error.
  url_loader_->SetAllowHttpErrorResults(true);

  network::SimpleURLLoader::HeadersOnlyCallback callback = base::BindOnce(
      &ConnectivityCheckerImpl::OnConnectivityCheckComplete, weak_this_);
  url_loader_->DownloadHeadersOnly(url_loader_factory_.get(),
                                   std::move(callback));
  // Exponential backoff for timeout in 3, 6 and 12 sec.
  const base::TimeDelta timeout =
      kRequestTimeout *
      std::pow(2, std::min(check_errors_, static_cast<unsigned int>(2)));
  timeout_.Reset(base::BindOnce(&ConnectivityCheckerImpl::OnUrlRequestTimeout,
                                weak_this_, timeout));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, timeout_.callback(), timeout);
}

void ConnectivityCheckerImpl::SetCastMetricsHelperForTesting(
    metrics::CastMetricsHelper* cast_metrics_helper) {
  DCHECK(cast_metrics_helper);
  cast_metrics_helper_ = cast_metrics_helper;
}

void ConnectivityCheckerImpl::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DVLOG(2) << "OnConnectionChanged " << type;
  connection_type_ = type;

  if (network_changed_pending_) {
    return;
  }
  if (std::exchange(first_connection_, false)) {
    OnConnectionChangedInternal();
    return;
  }
  network_changed_pending_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ConnectivityCheckerImpl::OnConnectionChangedInternal,
                     weak_this_),
      kNetworkChangedDelay);
}

void ConnectivityCheckerImpl::OnConnectionChangedInternal() {
  network_changed_pending_ = false;
  Cancel();

  if (connection_type_ == network::mojom::ConnectionType::CONNECTION_NONE) {
    SetConnected(false);
    return;
  }

  Check();
}

void ConnectivityCheckerImpl::OnTimeSynced() {
  SetConnected(network_connected_);
}

void ConnectivityCheckerImpl::OnConnectivityCheckComplete(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(url_loader_);

  // Move url_loader_ onto the stack to ensure it gets deleted when this
  // function completes.
  std::unique_ptr<network::SimpleURLLoader> url_loader = std::move(url_loader_);

  timeout_.Cancel();
  int error = url_loader->NetError();
  if (error == net::ERR_INSECURE_RESPONSE && url_loader->ResponseInfo() &&
      url_loader->ResponseInfo()->ssl_info) {
    LOG(ERROR) << "OnSSLCertificateError: cert_status="
               << url_loader->ResponseInfo()->ssl_info->cert_status;
    OnUrlRequestError(ErrorType::SSL_CERTIFICATE_ERROR);
    return;
  }
  if (error != net::OK) {
    // Captures non-HTTP errors here. All HTTP status codes (including error
    // codes) are treated as network success and won't be logged here. HTTP
    // errors are handled further below to provide more precise granularity.
    LOG(ERROR) << "Connectivity check failed: net_error=" << error;
    OnUrlRequestError(ErrorType::NET_ERROR);
    return;
  }

  // At this point, network connection is considered successful, but we still
  // need to check HTTP response for errors. If headers are empty, use an
  // implicit zero status code.
  int http_response_code = headers ? headers->response_code() : 0;

  if (http_response_code != kConnectivitySuccessStatusCode) {
    LOG(ERROR) << "Connectivity check failed: http_response_code="
               << http_response_code;
    OnUrlRequestError(ErrorType::BAD_HTTP_STATUS);
    return;
  }

  DVLOG(1) << "Connectivity check succeeded";
  check_errors_ = 0;
  SetConnected(true);
  // Some products don't have an idle screen that makes periodic network
  // requests. Schedule another check to ensure connectivity hasn't dropped.
  delayed_check_.Reset(
      base::BindOnce(&ConnectivityCheckerImpl::CheckInternal, weak_this_));
  task_runner_->PostDelayedTask(FROM_HERE, delayed_check_.callback(),
                                connected_probe_period_);
}

void ConnectivityCheckerImpl::OnUrlRequestError(ErrorType type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  ++check_errors_;
  if (check_errors_ > kNumErrorsToNotifyOffline) {
    LOG(INFO) << "Notify connectivity check failure.";
    NotifyCheckFailure();

    // Only record event on the connectivity transition.
    if (connected_and_time_synced_) {
      cast_metrics_helper_->RecordEventWithValue(
          kMetricNameNetworkConnectivityCheckingErrorType,
          static_cast<int>(type));
    }
    check_errors_ = kNumErrorsToNotifyOffline;
    SetConnected(false);
  }
  // Check again.
  delayed_check_.Reset(
      base::BindOnce(&ConnectivityCheckerImpl::CheckInternal, weak_this_));
  task_runner_->PostDelayedTask(FROM_HERE, delayed_check_.callback(),
                                disconnected_probe_period_);
}

void ConnectivityCheckerImpl::OnUrlRequestTimeout(base::TimeDelta timeout) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(url_loader_);
  url_loader_ = nullptr;
  LOG(WARNING) << "timed out after " << timeout;
  OnUrlRequestError(ErrorType::REQUEST_TIMEOUT);
}

void ConnectivityCheckerImpl::Cancel() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!url_loader_) {
    return;
  }
  VLOG(2) << "Cancel connectivity check in progress";
  url_loader_ = nullptr;
  timeout_.Cancel();
}

}  // namespace chromecast
