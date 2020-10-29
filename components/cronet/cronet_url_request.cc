// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/cronet_url_request.h"

#include <limits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/cronet/cronet_url_request_context.h"
#include "net/base/load_flags.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"

namespace cronet {

namespace {

// Returns the string representation of the HostPortPair of the proxy server
// that was used to fetch the response.
std::string GetProxy(const net::HttpResponseInfo& info) {
  if (!info.proxy_server.is_valid() || info.proxy_server.is_direct())
    return net::HostPortPair().ToString();
  return info.proxy_server.host_port_pair().ToString();
}

int CalculateLoadFlags(int load_flags,
                       bool disable_cache,
                       bool disable_connection_migration) {
  if (disable_cache)
    load_flags |= net::LOAD_DISABLE_CACHE;
  if (disable_connection_migration)
    load_flags |= net::LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  return load_flags;
}

}  // namespace

CronetURLRequest::CronetURLRequest(CronetURLRequestContext* context,
                                   std::unique_ptr<Callback> callback,
                                   const GURL& url,
                                   net::RequestPriority priority,
                                   bool disable_cache,
                                   bool disable_connection_migration,
                                   bool enable_metrics,
                                   bool traffic_stats_tag_set,
                                   int32_t traffic_stats_tag,
                                   bool traffic_stats_uid_set,
                                   int32_t traffic_stats_uid)
    : context_(context),
      network_tasks_(std::move(callback),
                     url,
                     priority,
                     CalculateLoadFlags(context->default_load_flags(),
                                        disable_cache,
                                        disable_connection_migration),
                     enable_metrics,
                     traffic_stats_tag_set,
                     traffic_stats_tag,
                     traffic_stats_uid_set,
                     traffic_stats_uid),
      initial_method_("GET"),
      initial_request_headers_(std::make_unique<net::HttpRequestHeaders>()) {
  DCHECK(!context_->IsOnNetworkThread());
}

CronetURLRequest::~CronetURLRequest() {
  DCHECK(context_->IsOnNetworkThread());
}

bool CronetURLRequest::SetHttpMethod(const std::string& method) {
  DCHECK(!context_->IsOnNetworkThread());
  // Http method is a token, just as header name.
  if (!net::HttpUtil::IsValidHeaderName(method))
    return false;
  initial_method_ = method;
  return true;
}

bool CronetURLRequest::AddRequestHeader(const std::string& name,
                                        const std::string& value) {
  DCHECK(!context_->IsOnNetworkThread());
  DCHECK(initial_request_headers_);
  if (!net::HttpUtil::IsValidHeaderName(name) ||
      !net::HttpUtil::IsValidHeaderValue(value)) {
    return false;
  }
  initial_request_headers_->SetHeader(name, value);
  return true;
}

void CronetURLRequest::SetUpload(
    std::unique_ptr<net::UploadDataStream> upload) {
  DCHECK(!context_->IsOnNetworkThread());
  DCHECK(!upload_);
  upload_ = std::move(upload);
}

void CronetURLRequest::Start() {
  DCHECK(!context_->IsOnNetworkThread());
  context_->PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(&CronetURLRequest::NetworkTasks::Start,
                     base::Unretained(&network_tasks_),
                     base::Unretained(context_), initial_method_,
                     std::move(initial_request_headers_), std::move(upload_)));
}

void CronetURLRequest::GetStatus(OnStatusCallback callback) const {
  context_->PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(&CronetURLRequest::NetworkTasks::GetStatus,
                     base::Unretained(&network_tasks_), std::move(callback)));
}

void CronetURLRequest::FollowDeferredRedirect() {
  context_->PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(&CronetURLRequest::NetworkTasks::FollowDeferredRedirect,
                     base::Unretained(&network_tasks_)));
}

bool CronetURLRequest::ReadData(net::IOBuffer* raw_read_buffer, int max_size) {
  scoped_refptr<net::IOBuffer> read_buffer(raw_read_buffer);
  context_->PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(&CronetURLRequest::NetworkTasks::ReadData,
                     base::Unretained(&network_tasks_), read_buffer, max_size));
  return true;
}

void CronetURLRequest::Destroy(bool send_on_canceled) {
  // Destroy could be called from any thread, including network thread (if
  // posting task to executor throws an exception), but is posted, so |this|
  // is valid until calling task is complete. Destroy() must be called from
  // within a synchronized block that guarantees no future posts to the
  // network thread with the request pointer.
  context_->PostTaskToNetworkThread(
      FROM_HERE, base::BindOnce(&CronetURLRequest::NetworkTasks::Destroy,
                                base::Unretained(&network_tasks_),
                                base::Unretained(this), send_on_canceled));
}

void CronetURLRequest::MaybeReportMetricsAndRunCallback(
    base::OnceClosure callback) {
  context_->PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(
          &CronetURLRequest::NetworkTasks::MaybeReportMetricsAndRunCallback,
          base::Unretained(&network_tasks_), std::move(callback)));
}

CronetURLRequest::NetworkTasks::NetworkTasks(std::unique_ptr<Callback> callback,
                                             const GURL& url,
                                             net::RequestPriority priority,
                                             int load_flags,
                                             bool enable_metrics,
                                             bool traffic_stats_tag_set,
                                             int32_t traffic_stats_tag,
                                             bool traffic_stats_uid_set,
                                             int32_t traffic_stats_uid)
    : callback_(std::move(callback)),
      initial_url_(url),
      initial_priority_(priority),
      initial_load_flags_(load_flags),
      received_byte_count_from_redirects_(0l),
      error_reported_(false),
      enable_metrics_(enable_metrics),
      metrics_reported_(false),
      traffic_stats_tag_set_(traffic_stats_tag_set),
      traffic_stats_tag_(traffic_stats_tag),
      traffic_stats_uid_set_(traffic_stats_uid_set),
      traffic_stats_uid_(traffic_stats_uid) {
  DETACH_FROM_THREAD(network_thread_checker_);
}

CronetURLRequest::NetworkTasks::~NetworkTasks() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
}

void CronetURLRequest::NetworkTasks::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  received_byte_count_from_redirects_ += request->GetTotalReceivedBytes();
  callback_->OnReceivedRedirect(
      redirect_info.new_url.spec(), redirect_info.status_code,
      request->response_headers()->GetStatusText(), request->response_headers(),
      request->response_info().was_cached,
      request->response_info().alpn_negotiated_protocol,
      GetProxy(request->response_info()), received_byte_count_from_redirects_);
  *defer_redirect = true;
}

void CronetURLRequest::NetworkTasks::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  // Cronet does not support client certificates.
  request->ContinueWithCertificate(nullptr, nullptr);
}

void CronetURLRequest::NetworkTasks::OnSSLCertificateError(
    net::URLRequest* request,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  ReportError(request, net_error);
  request->Cancel();
}

void CronetURLRequest::NetworkTasks::OnResponseStarted(net::URLRequest* request,
                                                       int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  if (net_error != net::OK) {
    ReportError(request, net_error);
    return;
  }
  callback_->OnResponseStarted(
      request->GetResponseCode(), request->response_headers()->GetStatusText(),
      request->response_headers(), request->response_info().was_cached,
      request->response_info().alpn_negotiated_protocol,
      GetProxy(request->response_info()),
      received_byte_count_from_redirects_ + request->GetTotalReceivedBytes());
}

void CronetURLRequest::NetworkTasks::OnReadCompleted(net::URLRequest* request,
                                                     int bytes_read) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  if (bytes_read < 0) {
    ReportError(request, bytes_read);
    return;
  }

  if (bytes_read == 0) {
    DCHECK(!error_reported_);
    MaybeReportMetrics();
    callback_->OnSucceeded(received_byte_count_from_redirects_ +
                           request->GetTotalReceivedBytes());
  } else {
    callback_->OnReadCompleted(
        read_buffer_, bytes_read,
        received_byte_count_from_redirects_ + request->GetTotalReceivedBytes());
  }
  // Free the read buffer.
  read_buffer_ = nullptr;
}

void CronetURLRequest::NetworkTasks::Start(
    CronetURLRequestContext* context,
    const std::string& method,
    std::unique_ptr<net::HttpRequestHeaders> request_headers,
    std::unique_ptr<net::UploadDataStream> upload) {
  DCHECK(context->IsOnNetworkThread());
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  VLOG(1) << "Starting chromium request: "
          << initial_url_.possibly_invalid_spec().c_str()
          << " priority: " << RequestPriorityToString(initial_priority_);
  url_request_ = context->GetURLRequestContext()->CreateRequest(
      initial_url_, net::DEFAULT_PRIORITY, this, MISSING_TRAFFIC_ANNOTATION);
  url_request_->SetLoadFlags(initial_load_flags_);
  url_request_->set_method(method);
  url_request_->SetExtraRequestHeaders(*request_headers);
  url_request_->SetPriority(initial_priority_);
  if (upload)
    url_request_->set_upload(std::move(upload));
  if (traffic_stats_tag_set_ || traffic_stats_uid_set_) {
#if defined(OS_ANDROID)
    url_request_->set_socket_tag(net::SocketTag(
        traffic_stats_uid_set_ ? traffic_stats_uid_ : net::SocketTag::UNSET_UID,
        traffic_stats_tag_set_ ? traffic_stats_tag_
                               : net::SocketTag::UNSET_TAG));
#else
    CHECK(false);
#endif
  }
  url_request_->Start();
}

void CronetURLRequest::NetworkTasks::GetStatus(
    OnStatusCallback callback) const {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  net::LoadState status = net::LOAD_STATE_IDLE;
  // |url_request_| is initialized in StartOnNetworkThread, and it is
  // never nulled. If it is null, it must be that StartOnNetworkThread
  // has not been called, pretend that we are in LOAD_STATE_IDLE.
  // See https://crbug.com/606872.
  if (url_request_)
    status = url_request_->GetLoadState().state;
  std::move(callback).Run(status);
}

void CronetURLRequest::NetworkTasks::FollowDeferredRedirect() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  url_request_->FollowDeferredRedirect(
      base::nullopt /* removed_request_headers */,
      base::nullopt /* modified_request_headers */);
}

void CronetURLRequest::NetworkTasks::ReadData(
    scoped_refptr<net::IOBuffer> read_buffer,
    int buffer_size) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  DCHECK(read_buffer);
  DCHECK(!read_buffer_);

  read_buffer_ = read_buffer;

  int result = url_request_->Read(read_buffer_.get(), buffer_size);
  // If IO is pending, wait for the URLRequest to call OnReadCompleted.
  if (result == net::ERR_IO_PENDING)
    return;

  OnReadCompleted(url_request_.get(), result);
}

void CronetURLRequest::NetworkTasks::Destroy(CronetURLRequest* request,
                                             bool send_on_canceled) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  MaybeReportMetrics();
  if (send_on_canceled)
    callback_->OnCanceled();
  callback_->OnDestroyed();
  // Deleting owner request also deletes |this|.
  delete request;
}

void CronetURLRequest::NetworkTasks::ReportError(net::URLRequest* request,
                                                 int net_error) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  DCHECK_LT(net_error, 0);
  DCHECK_EQ(request, url_request_.get());
  // Error may have already been reported.
  if (error_reported_)
    return;
  error_reported_ = true;
  net::NetErrorDetails net_error_details;
  url_request_->PopulateNetErrorDetails(&net_error_details);
  VLOG(1) << "Error " << net::ErrorToString(net_error)
          << " on chromium request: " << initial_url_.possibly_invalid_spec();
  MaybeReportMetrics();
  callback_->OnError(
      net_error, net_error_details.quic_connection_error,
      net::ErrorToString(net_error),
      received_byte_count_from_redirects_ + request->GetTotalReceivedBytes());
}

void CronetURLRequest::NetworkTasks::MaybeReportMetrics() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  // If there was an exception while starting the CronetUrlRequest, there won't
  // be a native URLRequest. In this case, the caller gets the exception
  // immediately, and the onFailed callback isn't called, so don't report
  // metrics either.
  if (!enable_metrics_ || metrics_reported_ || !url_request_) {
    return;
  }
  metrics_reported_ = true;
  net::LoadTimingInfo metrics;
  url_request_->GetLoadTimingInfo(&metrics);
  callback_->OnMetricsCollected(
      metrics.request_start_time, metrics.request_start,
      metrics.connect_timing.dns_start, metrics.connect_timing.dns_end,
      metrics.connect_timing.connect_start, metrics.connect_timing.connect_end,
      metrics.connect_timing.ssl_start, metrics.connect_timing.ssl_end,
      metrics.send_start, metrics.send_end, metrics.push_start,
      metrics.push_end, metrics.receive_headers_end, base::TimeTicks::Now(),
      metrics.socket_reused, url_request_->GetTotalSentBytes(),
      received_byte_count_from_redirects_ +
          url_request_->GetTotalReceivedBytes());
}

void CronetURLRequest::NetworkTasks::MaybeReportMetricsAndRunCallback(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  MaybeReportMetrics();
  std::move(callback).Run();
}

}  // namespace cronet
