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
#include "net/base/net_errors.h"
#include "net/http/http_version.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

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
  NSURLSession* session_override =
      url_session_test_util::GetTestURLSessionForConfig(std::move(config));
  url_loader->OverrideURLSessionForTesting(session_override);  // IN-TEST

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

  NSURLRequest* ns_request =
      url_session_helper::ConvertResourceRequest(request, timeout);
  NSURLSession* session = nil;
  if (nsurl_session_override_for_testing_) {
    CHECK_IS_TEST();
    session = nsurl_session_override_for_testing_;
  } else {
    session = [NSURLSession sharedSession];
  }

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
            SSORequestFailReason reason = error.code == NSURLErrorTimedOut
                                              ? SSORequestFailReason::kTimeout
                                              : SSORequestFailReason::kOsError;
            task_runner->PostTask(
                FROM_HERE, base::BindOnce(&URLSessionURLLoader::OnRequestFailed,
                                          weak_ptr, reason));
          } else if (!response) {
            LOG_POLICY(ERROR, EXTENSIBLE_SSO)
                << "[OktaEnterpriseSSO] URLSession request didn't fail but "
                   "didn't return a response";
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&URLSessionURLLoader::OnRequestFailed, weak_ptr,
                               SSORequestFailReason::kOther));
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
  auto head = url_session_helper::ConvertNSURLResponse(response);
  head->request_start = request_start_;
  head->response_start = base::TimeTicks::Now();

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
  base::UmaHistogramBoolean("Enterprise.ExtensibleEnterpriseSSO.Okta.Result",
                            true);
  base::TimeDelta duration = base::TimeTicks::Now() - request_start_;
  base::UmaHistogramTimes(
      "Enterprise.ExtensibleEnterpriseSSO.Okta.Success.Duration", duration);
}

void URLSessionURLLoader::RecordFailureMetrics(SSORequestFailReason reason) {
  base::UmaHistogramBoolean("Enterprise.ExtensibleEnterpriseSSO.Okta.Result",
                            false);
  base::UmaHistogramEnumeration(
      "Enterprise.ExtensibleEnterpriseSSO.Okta.Failure.Reason", reason);
  base::TimeDelta duration = base::TimeTicks::Now() - request_start_;
  base::UmaHistogramTimes(
      "Enterprise.ExtensibleEnterpriseSSO.Okta.Failure.Duration", duration);
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
