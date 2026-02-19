// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_URL_LOADER_H_
#define COMPONENTS_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_URL_LOADER_H_

#include <Foundation/Foundation.h>

#include "base/check_is_test.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_version.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/origin.h"

namespace enterprise_auth {

class URLSessionURLLoaderTest;

// URLLoader implementation for making requests with Apple's URLSession api.
// It is only meant for simple requests, received data over 1MiB will result in
// an error sent to the client.
// This class is self-owned, it will destroy itself if:
//  - the request is complete
//  - the client disconnects
//  - an error occurs with mojo or URLSession
// WARNING! This is meant to be used only for the Okta SSO flow, this class
// comes with multiple restrications and behaviour specific for this use case.
class COMPONENT_EXPORT(ENTERPRISE_PLATFORM_AUTH) URLSessionURLLoader
    : public network::mojom::URLLoader {
 public:
  URLSessionURLLoader(const URLSessionURLLoader&) = delete;
  URLSessionURLLoader& operator=(const URLSessionURLLoader&) = delete;

  static void CreateAndStart(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_info);

  static void CreateAndStartForTesting(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_info);

  // network::mojom::URLLoader
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const ::net::HttpRequestHeaders& modified_headers,
      const ::net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<::GURL>& new_url) override;

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;

  static constexpr char kTestServerResponseBody[] =
      "This is a test response body";

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(SSORequestFailReason)
  enum class SSORequestFailReason {
    kOther = 0,
    kOsError = 1,
    kTimeout = 2,
    kResponseTooBig = 3,
    kCorsViolation = 4,
    kMaxValue = kCorsViolation,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:OktaSSOFailureReason)

  static constexpr std::string_view kOktaResultHistogram =
      "Enterprise.ExtensibleEnterpriseSSO.Okta.Result";
  static constexpr std::string_view kOktaSuccessDurationHistogram =
      "Enterprise.ExtensibleEnterpriseSSO.Okta.Success.Duration";
  static constexpr std::string_view kOktaFailureDurationHistogram =
      "Enterprise.ExtensibleEnterpriseSSO.Okta.Failure.Duration";
  static constexpr std::string_view kOktaFailureReasonHistogram =
      "Enterprise.ExtensibleEnterpriseSSO.Okta.Failure.Reason";

 private:
  URLSessionURLLoader();
  ~URLSessionURLLoader() override;

  void Start(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_info_remote,
      base::TimeDelta timeout = kTimeout);

  void OnRequestComplete(NSURLResponse* response, NSData* data);

  void OnRequestFailed(SSORequestFailReason reason);

  void OnClientDisconnect();

  void OnReceiverDisconnect();

  void DisconnectAndDelete();

  void RecordSuccessMetrics();

  void RecordFailureMetrics(SSORequestFailReason reason);

  inline void SetAttachProtocolCallbackForTesting(
      base::OnceCallback<void(NSURLSessionConfiguration*)> callback) {
    CHECK_IS_TEST();
    attach_protocol_callback_for_testing_ = std::move(callback);
  }
  base::OnceCallback<void(NSURLSessionConfiguration*)>
      attach_protocol_callback_for_testing_;
  friend URLSessionURLLoaderTest;

  static constexpr base::TimeDelta kTimeout = base::Seconds(30);

  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  NSURLSessionTask* task_ = nil;
  base::TimeTicks request_start_;
  url::Origin request_initiator_;

  base::WeakPtrFactory<URLSessionURLLoader> weak_ptr_factory_{this};
};

}  // namespace enterprise_auth

#endif  // COMPONENTS_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_URL_LOADER_H_
