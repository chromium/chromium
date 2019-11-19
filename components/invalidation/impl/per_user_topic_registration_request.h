// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_REGISTRATION_REQUEST_H_
#define COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_REGISTRATION_REQUEST_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/invalidation/impl/status.h"
#include "components/invalidation/public/invalidation_util.h"
#include "net/http/http_request_headers.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace syncer {

// A single request to register against per user topic service.
class PerUserTopicRegistrationRequest {
 public:
  // The request result consists of the request status and name of the private
  // topic. The |topic_name| will be empty in the case of error.
  using CompletedCallback =
      base::OnceCallback<void(const Status& status,
                              const std::string& topic_name)>;
  enum RequestType { SUBSCRIBE, UNSUBSCRIBE };

  // Builds authenticated PerUserTopicRegistrationRequests.
  class Builder {
   public:
    Builder();
    Builder(Builder&&);
    ~Builder();

    // Builds a Request object in order to perform the registration.
    std::unique_ptr<PerUserTopicRegistrationRequest> Build() const;

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

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  ~PerUserTopicRegistrationRequest();

  // Starts an async request. The callback is invoked when the request succeeds
  // or fails. The callback is not called if the request is destroyed.
  void Start(CompletedCallback callback,
             network::mojom::URLLoaderFactory* loader_factory);

  GURL GetUrlForTesting() const { return url_; }

 private:
  PerUserTopicRegistrationRequest();
  void OnURLFetchComplete(std::unique_ptr<std::string> response_body);
  void OnURLFetchCompleteInternal(int net_error,
                                  int response_code,
                                  std::unique_ptr<std::string> response_body);

  void OnJsonParse(data_decoder::DataDecoder::ValueOrError result);

  // The fetcher for subscribing.
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  // The callback to notify when URLFetcher finished and results are available.
  // When the request is finished/aborted/destroyed, it's called in the dtor!
  CompletedCallback request_completed_callback_;

  // Full URL. Used in tests only.
  GURL url_;
  RequestType type_;
  std::string topic_;

  base::WeakPtrFactory<PerUserTopicRegistrationRequest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PerUserTopicRegistrationRequest);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_REGISTRATION_REQUEST_H_
