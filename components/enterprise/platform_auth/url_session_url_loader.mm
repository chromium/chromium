// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/enterprise/platform_auth/url_session_url_loader.h"

#include <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "base/byte_size.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/enterprise/platform_auth/url_session_helper.h"
#include "components/enterprise/platform_auth/url_session_test_util.h"
#include "components/policy/core/common/policy_logger.h"
#include "net/base/apple/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_version.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

// Helper class to enforce same-origin policies on redirects.
// URLSession's block-based API does not expose redirect handling, so a
// delegate must be used.
@interface URLSessionRedirectEnforcer : NSObject <NSURLSessionTaskDelegate>
- (instancetype)initWithAllowedOrigin:(url::Origin)origin;
@end

@implementation URLSessionRedirectEnforcer {
  url::Origin _allowedOrigin;
}

- (instancetype)initWithAllowedOrigin:(url::Origin)origin {
  if (self = [super init]) {
    _allowedOrigin = std::move(origin);
  }
  return self;
}

- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest*))completionHandler {
  const GURL newUrl = net::GURLWithNSURL(request.URL);
  if (!_allowedOrigin.IsSameOriginWith(newUrl)) {
    // Stops the redirect and returns to the original handler with the redirect
    // response.
    LOG_POLICY(ERROR, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] URLSession request blocked due to "
           "cross-origin redirect.";
    completionHandler(nil);
  } else {
    // Let the redirect through.
    completionHandler(request);
  }
}

@end

namespace enterprise_auth {

namespace {

constexpr base::ByteSize kDataSizeLimit = base::KiBU(128);

}  // namespace

URLSessionURLLoader::URLSessionURLLoader() = default;

URLSessionURLLoader::~URLSessionURLLoader() {
  if (task_) {
    [task_ cancel];
  }
}

// static
void URLSessionURLLoader::CreateAndStart(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_info) {
  // Lifetime of this class is self-managed, see url_session_url_loader.h for
  // more details.
  URLSessionURLLoader* url_loader = new URLSessionURLLoader();
  url_loader->Start(request, std::move(loader), std::move(client_info));
}

// static
void URLSessionURLLoader::CreateAndStartForTesting(  // IN-TEST
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_info) {
  CHECK_IS_TEST();
  // Lifetime of this class is self-managed, see url_session_url_loader.h for
  // more details.
  URLSessionURLLoader* url_loader = new URLSessionURLLoader();

  url_session_test_util::ResponseConfig config;
  config.body = kTestServerResponseBody;
  if (request.request_initiator.has_value()) {
    config.headers = {{"Access-Control-Allow-Origin",
                       request.request_initiator.value().Serialize()}};
  }
  url_loader->SetAttachProtocolCallbackForTesting(  // IN-TEST
      base::BindOnce(url_session_test_util::AttachProtocolToSessionForTesting,
                     std::move(config)));

  url_loader->Start(request, std::move(loader), std::move(client_info));
}

void URLSessionURLLoader::Start(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_info_remote,
    base::TimeDelta timeout) {
  VLOG_POLICY(2, EXTENSIBLE_SSO)
      << "[OktaEnterpriseSSO] Starting a URLSession request to "
      << request.url.spec();

  client_.Bind(std::move(client_info_remote));
  receiver_.Bind(std::move(loader));

  // base::Unretained is safe because the callbacks are owned by this object.
  receiver_.set_disconnect_handler(base::BindOnce(
      &URLSessionURLLoader::OnReceiverDisconnect, base::Unretained(this)));
  client_.set_disconnect_handler(base::BindOnce(
      &URLSessionURLLoader::OnClientDisconnect, base::Unretained(this)));

  request_start_ = base::TimeTicks::Now();

  if (!url_session_helper::IsOktaSSORequest(request)) {
    LOG_POLICY(WARNING, EXTENSIBLE_SSO)
        << "URLSessionURLLoader started for a non Okta SSO request.";
    OnRequestFailed(SSORequestFailReason::kOther);
    return;
  }

  if (!request.request_initiator.has_value()) {
    LOG_POLICY(ERROR, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] URLSessionURLLoader expects the caller to "
           "verify that |request| "
           "contains a valid |request_initiator|.";
    OnRequestFailed(SSORequestFailReason::kOther);
    return;
  }

  if (!request.request_initiator->IsSameOriginWith(request.url)) {
    LOG_POLICY(ERROR, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] URLSessionURLLoader cross origin request "
           "attempted.";
    OnRequestFailed(SSORequestFailReason::kCorsViolation);
    return;
  }

  request_initiator_ = request.request_initiator.value();

  NSURLRequest* ns_request =
      url_session_helper::ConvertResourceRequest(request, timeout);
  // To enforce same-origin policies on redirects, we must create a specific
  // session with a delegate that intercepts redirects.
  // Ephemeral session acts like a default session but doesn't write anything
  // to disk.
  URLSessionRedirectEnforcer* bridge = [[URLSessionRedirectEnforcer alloc]
      initWithAllowedOrigin:request.request_initiator.value()];
  NSURLSessionConfiguration* config =
      [NSURLSessionConfiguration ephemeralSessionConfiguration];
  if (attach_protocol_callback_for_testing_) {
    CHECK_IS_TEST();
    std::move(attach_protocol_callback_for_testing_).Run(config);
  }
  NSURLSession* session = [NSURLSession sessionWithConfiguration:config
                                                        delegate:bridge
                                                   delegateQueue:nil];

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  // This completionHandler will run on Apple's network thread.
  // It captures task_runner and weak_ptr by copy.
  task_ = [session
      dataTaskWithRequest:ns_request
        completionHandler:^(NSData* data, NSURLResponse* response,
                            NSError* error) {
          if (error) {
            LOG_POLICY(ERROR, EXTENSIBLE_SSO)
                << "[OktaEnterpriseSSO] URLSession request failed with code: "
                << error.code;
            const SSORequestFailReason reason =
                error.code == NSURLErrorTimedOut
                    ? SSORequestFailReason::kTimeout
                    : SSORequestFailReason::kOsError;
            task_runner->PostTask(
                FROM_HERE, base::BindOnce(&URLSessionURLLoader::OnRequestFailed,
                                          weak_ptr, reason));
          } else if (!response) {
            // Happens when the redirect is interrupted.
            LOG_POLICY(ERROR, EXTENSIBLE_SSO)
                << "[OktaEnterpriseSSO] URLSession completion handler received "
                   "no error but response was nil. This was most likely caused "
                   "by a blocked corss-origin redirect.";
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&URLSessionURLLoader::OnRequestFailed, weak_ptr,
                               SSORequestFailReason::kCorsViolation));
          } else {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&URLSessionURLLoader::OnRequestComplete,
                               weak_ptr, response, data));
          }
        }];

  // Starts the request asynchronously.
  [task_ resume];
}

void URLSessionURLLoader::OnRequestComplete(NSURLResponse* response,
                                            NSData* ns_data) {
  task_ = nil;
  if (!client_) {
    // At this point, it is possible for |client_| to have disconnected, but
    // the callback might still be pending in the task queue.
    return;
  }

  // |response| is guarenteed to be valid.
  auto head =
      url_session_helper::ConvertNSURLResponse(response, request_initiator_);
  if (!head) {
    // If the conversion failed that means that CORS headers didn't match
    // expectations.
    OnRequestFailed(SSORequestFailReason::kCorsViolation);
    return;
  }
  head->request_start = request_start_;
  head->response_start = base::TimeTicks::Now();

  if (head->headers->response_code() >= 300 &&
      head->headers->response_code() < 400) {
    // It might be the case that interrupted redirect returns the redirect
    // response.
    LOG_POLICY(ERROR, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] URLSession request blocked due to "
           "cross-origin redirect.";
    OnRequestFailed(SSORequestFailReason::kCorsViolation);
    return;
  }

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  if (ns_data && ns_data.length > 0) {
    if (ns_data.length > kDataSizeLimit.InBytes()) {
      LOG_POLICY(ERROR, EXTENSIBLE_SSO)
          << "[OktaEnterpriseSSO] URLSession response body too big. Received "
          << ns_data.length << " bytes.";
      OnRequestFailed(SSORequestFailReason::kResponseTooBig);
      return;
    }

    const base::span<const uint8_t> data = base::apple::NSDataToSpan(ns_data);
    mojo::ScopedDataPipeProducerHandle producer_handle;
    int mojo_result =
        mojo::CreateDataPipe(data.size(), producer_handle, consumer_handle);
    if (mojo_result != MOJO_RESULT_OK) {
      LOG_POLICY(ERROR, EXTENSIBLE_SSO)
          << "[OktaEnterpriseSSO] Mojo data pipe creation failed with code: "
          << mojo_result;
      OnRequestFailed(SSORequestFailReason::kOther);
      return;
    }
    mojo_result = producer_handle->WriteAllData(data);
    if (mojo_result != MOJO_RESULT_OK) {
      LOG_POLICY(ERROR, EXTENSIBLE_SSO)
          << "[OktaEnterpriseSSO] Writing to the mojo pipe failed with code "
          << mojo_result;
      OnRequestFailed(SSORequestFailReason::kOther);
      return;
    }
  }

  VLOG_POLICY(2, EXTENSIBLE_SSO)
      << "[OktaEnterpriseSSO] URLSession request completed successfully";
  RecordSuccessMetrics();
  client_->OnReceiveResponse(std::move(head), std::move(consumer_handle),
                             std::nullopt);
  client_->OnComplete(network::URLLoaderCompletionStatus(net::OK));
  DisconnectAndDelete();
}

void URLSessionURLLoader::OnRequestFailed(SSORequestFailReason reason) {
  RecordFailureMetrics(reason);

  task_ = nil;
  if (!client_) {
    // At this point, it is possible for |client_| to have disconnected, but
    // the callback might still be pending in the task queue.
    return;
  }
  int error_code;
  switch (reason) {
    case SSORequestFailReason::kTimeout:
      error_code = net::ERR_TIMED_OUT;
      break;
    case SSORequestFailReason::kResponseTooBig:
      error_code = net::ERR_FILE_TOO_BIG;
      break;
    case SSORequestFailReason::kOsError:
    case SSORequestFailReason::kOther:
    case SSORequestFailReason::kCorsViolation:
      error_code = net::ERR_FAILED;
      break;
  }
  client_->OnComplete(network::URLLoaderCompletionStatus(error_code));
  DisconnectAndDelete();
}

void URLSessionURLLoader::OnClientDisconnect() {
  DisconnectAndDelete();
}

void URLSessionURLLoader::OnReceiverDisconnect() {
  // The receiver might have disconnected but the client might still be waiting
  // for the response, so we don't delete and disconnect just yet.
  receiver_.reset();
}

void URLSessionURLLoader::DisconnectAndDelete() {
  client_.reset();
  receiver_.reset();
  delete this;
}

void URLSessionURLLoader::RecordSuccessMetrics() {
  base::UmaHistogramBoolean(kOktaResultHistogram, true);
  base::TimeDelta duration = base::TimeTicks::Now() - request_start_;
  base::UmaHistogramTimes(kOktaSuccessDurationHistogram, duration);
}

void URLSessionURLLoader::RecordFailureMetrics(SSORequestFailReason reason) {
  base::UmaHistogramBoolean(kOktaResultHistogram, false);
  base::UmaHistogramEnumeration(kOktaFailureReasonHistogram, reason);
  base::TimeDelta duration = base::TimeTicks::Now() - request_start_;
  base::UmaHistogramTimes(kOktaFailureDurationHistogram, duration);
}

// We let URLSession follow redirects and only get the final result.
void URLSessionURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const ::net::HttpRequestHeaders& modified_headers,
    const ::net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<::GURL>& new_url) {
  NOTREACHED();
}

// Does not apply to URLSession.
void URLSessionURLLoader::SetPriority(net::RequestPriority priority,
                                      int32_t intra_priority_value) {}

}  // namespace enterprise_auth
