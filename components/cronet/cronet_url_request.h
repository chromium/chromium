// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_CRONET_URL_REQUEST_H_
#define COMPONENTS_CRONET_CRONET_URL_REQUEST_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace net {
class HttpRequestHeaders;
enum LoadState;
class SSLCertRequestInfo;
class SSLInfo;
class UploadDataStream;
}  // namespace net

namespace cronet {

class CronetURLRequestContext;
class TestUtil;

// Wrapper around net::URLRequestContext.
// Created and configured from client thread. Start, ReadData, and Destroy are
// posted to network thread and all callbacks into the Callback() are
// done on the network thread. CronetUrlRequest client is expected to initiate
// the next step like FollowDeferredRedirect, ReadData or Destroy. Public
// methods can be called on any thread.
class CronetURLRequest {
 public:
  // Callback implemented by CronetURLRequest() caller and owned by
  // CronetURLRequest::NetworkTasks. All callback methods are invoked on network
  // thread.
  class Callback {
   public:
    virtual ~Callback() = default;

    // Invoked whenever a redirect is encountered. This will only be invoked
    // between the call to CronetURLRequest::Start() and
    // Callback::OnResponseStarted(). The body of the redirect response, if
    // it has one, will be ignored.
    //
    // The redirect will not be followed until
    // CronetURLRequest::FollowDeferredRedirect() method is called, either
    // synchronously or asynchronously.
    virtual void OnReceivedRedirect(const std::string& new_location,
                                    int http_status_code,
                                    const std::string& http_status_text,
                                    const net::HttpResponseHeaders* headers,
                                    bool was_cached,
                                    const std::string& negotiated_protocol,
                                    const std::string& proxy_server,
                                    int64_t received_byte_count) = 0;

    // Invoked when the final set of headers, after all redirects, is received.
    // Will only be invoked once for each request.
    //
    // With the exception of Callback::OnCanceled(),
    // no other Callback method will be invoked for the request,
    // including Callback::OnSucceeded() and Callback::OnFailed(), until
    // CronetUrlRequest::Read() is called to attempt to start reading the
    // response body.
    virtual void OnResponseStarted(int http_status_code,
                                   const std::string& http_status_text,
                                   const net::HttpResponseHeaders* headers,
                                   bool was_cached,
                                   const std::string& negotiated_protocol,
                                   const std::string& proxy_server,
                                   int64_t received_byte_count) = 0;

    // Invoked whenever part of the response body has been read. Only part of
    // the buffer may be populated, even if the entire response body has not yet
    // been consumed.
    //
    // With the exception of Callback::OnCanceled(),
    // no other Callback method will be invoked for the request,
    // including Callback::OnSucceeded() and Callback::OnFailed(), until
    // CronetUrlRequest::Read() is called to attempt to continue reading the
    // response body.
    virtual void OnReadCompleted(scoped_refptr<net::IOBuffer> buffer,
                                 int bytes_read,
                                 int64_t received_byte_count) = 0;

    // Invoked when request is completed successfully.
    virtual void OnSucceeded(int64_t received_byte_count) = 0;

    // Invoked if request failed for any reason after CronetURLRequest::Start().
    // |net_error| provides information about the failure. |quic_error| is only
    // valid if |net_error| is net::QUIC_PROTOCOL_ERROR.
    virtual void OnError(int net_error,
                         int quic_error,
                         const std::string& error_string,
                         int64_t received_byte_count) = 0;

    // Invoked if request was canceled via CronetURLRequest::Destroy().
    virtual void OnCanceled() = 0;

    // Invoked when request is destroyed. Once invoked, no other Callback
    // methods will be invoked.
    virtual void OnDestroyed() = 0;

    // Invoked right before request is destroyed to report collected metrics if
    // |enable_metrics| is true in CronetURLRequest::CronetURLRequest().
    virtual void OnMetricsCollected(const base::Time& request_start_time,
                                    const base::TimeTicks& request_start,
                                    const base::TimeTicks& dns_start,
                                    const base::TimeTicks& dns_end,
                                    const base::TimeTicks& connect_start,
                                    const base::TimeTicks& connect_end,
                                    const base::TimeTicks& ssl_start,
                                    const base::TimeTicks& ssl_end,
                                    const base::TimeTicks& send_start,
                                    const base::TimeTicks& send_end,
                                    const base::TimeTicks& push_start,
                                    const base::TimeTicks& push_end,
                                    const base::TimeTicks& receive_headers_end,
                                    const base::TimeTicks& request_end,
                                    bool socket_reused,
                                    int64_t sent_bytes_count,
                                    int64_t received_bytes_count) = 0;
  };
  // Invoked in response to CronetURLRequest::GetStatus() to allow multiple
  // overlapping calls. The load states correspond to the lengthy periods of
  // time that a request load may be blocked and unable to make progress.
  using OnStatusCallback = base::OnceCallback<void(net::LoadState)>;

  // Bypasses cache if |disable_cache| is true. If context is not set up to
  // use cache, |disable_cache| has no effect. |disable_connection_migration|
  // causes connection migration to be disabled for this request if true. If
  // global connection migration flag is not enabled,
  // |disable_connection_migration| has no effect.
  CronetURLRequest(CronetURLRequestContext* context,
                   std::unique_ptr<Callback> callback,
                   const GURL& url,
                   net::RequestPriority priority,
                   bool disable_cache,
                   bool disable_connection_migration,
                   bool enable_metrics,
                   bool traffic_stats_tag_set,
                   int32_t traffic_stats_tag,
                   bool traffic_stats_uid_set,
                   int32_t traffic_stats_uid);

  // Methods called prior to Start are never called on network thread.

  // Sets the request method GET, POST etc.
  bool SetHttpMethod(const std::string& method);

  // Adds a header to the request before it starts.
  bool AddRequestHeader(const std::string& name, const std::string& value);

  // Adds a request body to the request before it starts.
  void SetUpload(std::unique_ptr<net::UploadDataStream> upload);

  // Starts the request.
  void Start();

  // GetStatus invokes |on_status_callback| on network thread to allow multiple
  // overlapping calls.
  void GetStatus(OnStatusCallback on_status_callback) const;

  // Follows redirect.
  void FollowDeferredRedirect();

  // Reads more data.
  bool ReadData(net::IOBuffer* buffer, int max_bytes);

  // Releases all resources for the request and deletes the object itself.
  // |send_on_canceled| indicates whether OnCanceled callback should be
  // issued to indicate when no more callbacks will be issued.
  void Destroy(bool send_on_canceled);

  // On the network thread, reports metrics to the registered
  // CronetURLRequest::Callback, and then runs |callback| on the network thread.
  //
  // Since metrics are only reported once, this can be used to ensure metrics
  // are reported to the registered CronetURLRequest::Callback before resources
  // used by the callback are deleted.
  void MaybeReportMetricsAndRunCallback(base::OnceClosure callback);

 private:
  friend class TestUtil;

  // Private destructor invoked fron NetworkTasks::Destroy() on network thread.
  ~CronetURLRequest();

  // NetworkTasks performs tasks on the network thread and owns objects that
  // live on the network thread.
  class NetworkTasks : public net::URLRequest::Delegate {
   public:
    // Invoked off the network thread.
    NetworkTasks(std::unique_ptr<Callback> callback,
                 const GURL& url,
                 net::RequestPriority priority,
                 int load_flags,
                 bool enable_metrics,
                 bool traffic_stats_tag_set,
                 int32_t traffic_stats_tag,
                 bool traffic_stats_uid_set,
                 int32_t traffic_stats_uid);

    // Invoked on the network thread.
    ~NetworkTasks() override;

    // Starts the request.
    void Start(CronetURLRequestContext* context,
               const std::string& method,
               std::unique_ptr<net::HttpRequestHeaders> request_headers,
               std::unique_ptr<net::UploadDataStream> upload);

    // Gets status of the requrest and invokes |on_status_callback| to allow
    // multiple overlapping calls.
    void GetStatus(OnStatusCallback on_status_callback) const;

    // Follows redirect.
    void FollowDeferredRedirect();

    // Reads more data.
    void ReadData(scoped_refptr<net::IOBuffer> read_buffer, int buffer_size);

    // Releases all resources for the request and deletes the |request|, which
    // owns |this|, so |this| is also deleted.
    // |send_on_canceled| indicates whether OnCanceled callback should be
    // issued to indicate when no more callbacks will be issued.
    void Destroy(CronetURLRequest* request, bool send_on_canceled);

    // Runs MaybeReportMetrics(), then runs |callback|.
    void MaybeReportMetricsAndRunCallback(base::OnceClosure callback);

   private:
    friend class TestUtil;

    // net::URLRequest::Delegate implementations:
    void OnReceivedRedirect(net::URLRequest* request,
                            const net::RedirectInfo& redirect_info,
                            bool* defer_redirect) override;
    void OnCertificateRequested(
        net::URLRequest* request,
        net::SSLCertRequestInfo* cert_request_info) override;
    void OnSSLCertificateError(net::URLRequest* request,
                               int net_error,
                               const net::SSLInfo& ssl_info,
                               bool fatal) override;
    void OnResponseStarted(net::URLRequest* request, int net_error) override;
    void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

    // Report error and cancel request_adapter.
    void ReportError(net::URLRequest* request, int net_error);
    // Reports metrics collected.
    void MaybeReportMetrics();

    // Callback implemented by the client.
    std::unique_ptr<CronetURLRequest::Callback> callback_;

    const GURL initial_url_;
    const net::RequestPriority initial_priority_;
    const int initial_load_flags_;
    // Count of bytes received during redirect is added to received byte count.
    int64_t received_byte_count_from_redirects_;

    // Whether error has been already reported, for example from
    // OnSSLCertificateError().
    bool error_reported_;

    // Whether detailed metrics should be collected and reported.
    const bool enable_metrics_;
    // Whether metrics have been reported.
    bool metrics_reported_;

    // Whether |traffic_stats_tag_| should be applied.
    const bool traffic_stats_tag_set_;
    // TrafficStats tag to apply to URLRequest.
    const int32_t traffic_stats_tag_;
    // Whether |traffic_stats_uid_| should be applied.
    const bool traffic_stats_uid_set_;
    // UID to be applied to URLRequest.
    const int32_t traffic_stats_uid_;

    scoped_refptr<net::IOBuffer> read_buffer_;
    std::unique_ptr<net::URLRequest> url_request_;

    THREAD_CHECKER(network_thread_checker_);
    DISALLOW_COPY_AND_ASSIGN(NetworkTasks);
  };

  CronetURLRequestContext* context_;
  // |network_tasks_| is invoked on network thread.
  NetworkTasks network_tasks_;

  // Request parameters set off network thread before Start().
  std::string initial_method_;
  std::unique_ptr<net::HttpRequestHeaders> initial_request_headers_;
  std::unique_ptr<net::UploadDataStream> upload_;

  DISALLOW_COPY_AND_ASSIGN(CronetURLRequest);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_CRONET_URL_REQUEST_H_
