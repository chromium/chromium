// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/connectivity_checker_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/net/net_switches.h"
#include "net/base/request_priority.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/http/http_transaction_factory.h"
#include "net/socket/ssl_client_socket.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"

namespace chromecast {

namespace {

// How often connectivity checks are performed in seconds while not connected.
const unsigned int kConnectivityPeriodSeconds = 1;

// How often connectivity checks are performed in seconds while connected.
const unsigned int kConnectivitySuccessPeriodSeconds = 60;

// Number of consecutive connectivity check errors before status is changed
// to offline.
const unsigned int kNumErrorsToNotifyOffline = 3;

// Request timeout value in seconds.
const unsigned int kRequestTimeoutInSeconds = 3;

// Default url for connectivity checking.
const char kDefaultConnectivityCheckUrl[] =
    "https://connectivitycheck.gstatic.com/generate_204";

// Http url for connectivity checking.
const char kHttpConnectivityCheckUrl[] =
    "http://connectivitycheck.gstatic.com/generate_204";

// Delay notification of network change events to smooth out rapid flipping.
// Histogram "Cast.Network.Down.Duration.In.Seconds" shows 40% of network
// downtime is less than 3 seconds.
const char kNetworkChangedDelayInSeconds = 3;

const char kMetricNameNetworkConnectivityCheckingErrorType[] =
    "Network.ConnectivityChecking.ErrorType";

}  // namespace

// static
scoped_refptr<ConnectivityCheckerImpl> ConnectivityCheckerImpl::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    network::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK(task_runner);

  auto connectivity_checker = base::WrapRefCounted(
      new ConnectivityCheckerImpl(task_runner, network_connection_tracker));
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&ConnectivityCheckerImpl::Initialize, connectivity_checker,
                     std::move(url_loader_factory_info)));
  return connectivity_checker;
}

ConnectivityCheckerImpl::ConnectivityCheckerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    network::NetworkConnectionTracker* network_connection_tracker)
    : ConnectivityChecker(task_runner),
      task_runner_(std::move(task_runner)),
      network_connection_tracker_(network_connection_tracker),
      connected_(false),
      connection_type_(network::mojom::ConnectionType::CONNECTION_NONE),
      check_errors_(0),
      network_changed_pending_(false),
      weak_factory_(this) {
  DCHECK(task_runner_);
  DCHECK(network_connection_tracker_);
  weak_this_ = weak_factory_.GetWeakPtr();
}

void ConnectivityCheckerImpl::Initialize(
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(url_loader_factory_info);
  url_loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::move(url_loader_factory_info));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringType check_url_str =
      command_line->GetSwitchValueNative(switches::kConnectivityCheckUrl);
  connectivity_check_url_.reset(new GURL(
      check_url_str.empty() ? kDefaultConnectivityCheckUrl : check_url_str));

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
  return connected_;
}

void ConnectivityCheckerImpl::SetConnected(bool connected) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock auto_lock(connected_lock_);
    if (connected_ == connected) {
      return;
    }
    connected_ = connected;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringType check_url_str =
      command_line->GetSwitchValueNative(switches::kConnectivityCheckUrl);
  if (check_url_str.empty()) {
    connectivity_check_url_.reset(new GURL(
      connected ? kHttpConnectivityCheckUrl : kDefaultConnectivityCheckUrl));
    LOG(INFO) << "Change check url=" << *connectivity_check_url_;
  }

  Notify(connected);
  LOG(INFO) << "Global connection is: " << (connected ? "Up" : "Down");
}

void ConnectivityCheckerImpl::Check() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ConnectivityCheckerImpl::CheckInternal, weak_this_));
}

void ConnectivityCheckerImpl::CheckInternal() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(url_loader_factory_);

  // Don't check connectivity if network is offline, because Internet could be
  // accessible via netifs ignored.
  auto connection_type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  network_connection_tracker_->GetConnectionType(
      &connection_type,
      base::BindOnce(&ConnectivityCheckerImpl::OnConnectionChanged,
                     weak_this_));
  if (connection_type == network::mojom::ConnectionType::CONNECTION_NONE) {
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
  // To enable ssl_info in the response.
  resource_request->report_raw_headers = true;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 MISSING_TRAFFIC_ANNOTATION);
  network::SimpleURLLoader::HeadersOnlyCallback callback = base::BindOnce(
      &ConnectivityCheckerImpl::OnConnectivityCheckComplete, weak_this_);
  url_loader_->DownloadHeadersOnly(url_loader_factory_.get(),
                                   std::move(callback));

  timeout_.Reset(
      base::Bind(&ConnectivityCheckerImpl::OnUrlRequestTimeout, weak_this_));
  // Exponential backoff for timeout in 3, 6 and 12 sec.
  const int timeout = kRequestTimeoutInSeconds
                      << (check_errors_ > 2 ? 2 : check_errors_);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, timeout_.callback(), base::TimeDelta::FromSeconds(timeout));
}

void ConnectivityCheckerImpl::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DVLOG(2) << "OnConnectionChanged " << type;
  connection_type_ = type;

  if (network_changed_pending_)
    return;
  network_changed_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ConnectivityCheckerImpl::OnConnectionChangedInternal,
                     weak_this_),
      base::TimeDelta::FromSeconds(kNetworkChangedDelayInSeconds));
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

void ConnectivityCheckerImpl::OnConnectivityCheckComplete(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(url_loader_);
  timeout_.Cancel();
  int error = url_loader_->NetError();
  if (error == net::ERR_INSECURE_RESPONSE && url_loader_->ResponseInfo() &&
      url_loader_->ResponseInfo()->ssl_info) {
    LOG(ERROR) << "OnSSLCertificateError: cert_status="
               << url_loader_->ResponseInfo()->ssl_info->cert_status;
    OnUrlRequestError(ErrorType::SSL_CERTIFICATE_ERROR);
    return;
  }
  int http_response_code = (error == net::OK && headers)
                               ? headers->response_code()
                               : net::HTTP_BAD_REQUEST;

  // Clears resources.
  url_loader_.reset(nullptr);

  if (http_response_code < 400) {
    DVLOG(1) << "Connectivity check succeeded";
    check_errors_ = 0;
    SetConnected(true);
    // Some products don't have an idle screen that makes periodic network
    // requests. Schedule another check to ensure connectivity hasn't dropped.
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ConnectivityCheckerImpl::CheckInternal, weak_this_),
        base::TimeDelta::FromSeconds(kConnectivitySuccessPeriodSeconds));
    return;
  }
  LOG(ERROR) << "Connectivity check failed: " << http_response_code;
  OnUrlRequestError(ErrorType::BAD_HTTP_STATUS);
}

void ConnectivityCheckerImpl::OnUrlRequestError(ErrorType type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  ++check_errors_;
  if (check_errors_ > kNumErrorsToNotifyOffline) {
    // Only record event on the connectivity transition.
    if (connected_) {
      metrics::CastMetricsHelper::GetInstance()->RecordEventWithValue(
          kMetricNameNetworkConnectivityCheckingErrorType,
          static_cast<int>(type));
    }
    check_errors_ = kNumErrorsToNotifyOffline;
    SetConnected(false);
  }
  url_loader_.reset(nullptr);
  // Check again.
  task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&ConnectivityCheckerImpl::Check, weak_this_),
      base::TimeDelta::FromSeconds(kConnectivityPeriodSeconds));
}

void ConnectivityCheckerImpl::OnUrlRequestTimeout() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  LOG(ERROR) << "time out";
  OnUrlRequestError(ErrorType::REQUEST_TIMEOUT);
}

void ConnectivityCheckerImpl::Cancel() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!url_loader_)
    return;
  VLOG(2) << "Cancel connectivity check in progress";
  url_loader_.reset(nullptr);
  timeout_.Cancel();
}

}  // namespace chromecast
