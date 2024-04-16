// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_SUBSCRIPTION_REQUEST_H_
#define COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_SUBSCRIPTION_REQUEST_H_

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/invalidation/impl/status.h"
#include "components/invalidation/public/invalidation_util.h"
#include "net/http/http_request_headers.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace invalidation {

// A single request to subscribe to a topic on the per-user-topic service.
class PerUserTopicSubscriptionRequest {
 public:
  // The request result consists of the request status and name of the private
  // topic. The |topic_name| will be empty in the case of error.
  using CompletedCallback =
      base::OnceCallback<void(const Status& status,
                              const std::string& topic_name)>;
  enum class RequestType { kSubscribe, kUnsubscribe };

  // Builds authenticated PerUserTopicSubscriptionRequests.
  class Builder {
   public:
    Builder();
    Builder(const Builder& other) = delete;
    Builder& operator=(const Builder& other) = delete;
    ~Builder();

    // Builds a Request object in order to perform the subscription.
    std::unique_ptr<PerUserTopicSubscriptionRequest> Build() const;

    Builder& SetInstanceIdToken(const std::string& token);
    Builder& SetScope(const std::string& scope);
    Builder& SetAuthenticationHeader(const std::string& auth_header);

    Builder& SetPublicTopicName(const Topic& topic);
    Builder& SetProjectId(const std::string& project_id);

    Builder& SetType(RequestType type);

    Builder& SetTopicIsPublic(bool topic_is_public);

   private:
    net::HttpRequestHeaders BuildHeaders() const;
    std::string BuildBody() const;
    std::unique_ptr<network::SimpleURLLoader> BuildURLFetcher(
        const net::HttpRequestHeaders& headers,
        const std::string& body,
        const GURL& url) const;

    // GCM subscription token obtained from GCM driver (instanceID::getToken()).
    std::string instance_id_token_;
    Topic topic_;
    std::string project_id_;

    std::string scope_;
    std::string auth_header_;
    RequestType type_;
    bool topic_is_public_ = false;
  };

  PerUserTopicSubscriptionRequest(
      const PerUserTopicSubscriptionRequest& other) = delete;
  PerUserTopicSubscriptionRequest& operator=(
      const PerUserTopicSubscriptionRequest& other) = delete;
  ~PerUserTopicSubscriptionRequest();

  // Starts an async request. The callback is invoked when the request succeeds
  // or fails. The callback is not called if the request is destroyed.
  void Start(CompletedCallback callback,
             network::mojom::URLLoaderFactory* loader_factory);

  GURL GetUrlForTesting() const { return url_; }

 private:
  PerUserTopicSubscriptionRequest();

  // The methods below may end up calling RunCompletedCallbackAndMaybeDie(),
  // which potentially lead to destroying |this|. Hence, |this| object must
  // assume that it is dead after invoking any of these methods and must not
  // run any more code.
  void OnURLFetchComplete(std::unique_ptr<std::string> response_body);
  void OnURLFetchCompleteInternal(int net_error,
                                  int response_code,
                                  std::unique_ptr<std::string> response_body);
  void OnJsonParse(data_decoder::DataDecoder::ValueOrError result);

  // Invokes |request_completed_callback_| with (|status|, |topic_name|). Per
  // the contract of this class, it is allowed for clients to delete this
  // object as part of the invocation of |request_completed_callback_|. Hence,
  // this object must assume that it is dead after invoking this method and
  // must not run any more code. See crbug.com/1054590 as sample issue for
  // violation of this rule.
  // |status| and |topic_name| are intentionally taken by value to avoid
  // references to members.
  void RunCompletedCallbackAndMaybeDie(Status status, std::string topic_name);
  // The fetcher for subscribing.
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  // The callback to notify when URLFetcher finished and results are available.
  // When the request is finished/aborted/destroyed, it's called in the dtor!
  // Note: This callback should only be invoked from
  // RunCompletedCallbackAndMaybeDie(), as invoking it has the potential to
  // destroy this object per this class's contract.
  // TODO(crbug.com/40675891): find a way to avoid this fragile logic.
  CompletedCallback request_completed_callback_;

  // Full URL. Used in tests only.
  GURL url_;
  RequestType type_;
  std::string topic_;

  base::WeakPtrFactory<PerUserTopicSubscriptionRequest> weak_ptr_factory_{this};
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_SUBSCRIPTION_REQUEST_H_
