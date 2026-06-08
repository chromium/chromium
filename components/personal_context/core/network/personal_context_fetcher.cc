// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/network/personal_context_fetcher.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"

namespace personal_context {

namespace {

constexpr char kAuthorizationBearerPrefix[] = "Bearer ";
constexpr char kFetchContextMethod[] = ":fetchContext";
constexpr char kFetchPiiEntitiesMethod[] = ":fetchPiiEntities";
constexpr char kGoogleAPITypeName[] = "type.googleapis.com/";
constexpr char kHttpPostMethod[] = "POST";
constexpr char kRequestContentType[] = "application/x-protobuf";
constexpr char kServerTimeoutHeader[] = "X-Server-Timeout";

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    proto::ContextMemoryFeature feature) {
  switch (feature) {
    case proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL:
      return net::DefineNetworkTrafficAnnotation("ambient_autofill_request", R"(
          semantics {
            sender: "Ambient Autofill"
            description:
              "Fetches ambient autofill suggestions from the Context Memory "
              "Service. This is used to provide autofill suggestions (such as "
              "passports, driver's licenses, flight reservations, etc.) based "
              "on the user's personal context and the current webpage's domain."
            trigger:
              "User interacts with a form field that supports ambient autofill."
            destination: GOOGLE_OWNED_SERVICE
            data:
              "The domain of the current webpage and the requested entity "
              "types."
            user_data {
              type: ACCESS_TOKEN
              type: SENSITIVE_URL
            }
            last_reviewed: "2026-05-26"
            internal {
              contacts {
                email: "1p-integrations-autofill@google.com"
              }
            }
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can enable or disable Autofill AI in Chrome settings "
              "under 'Autofill Settings' -> 'Enhanced Autofill'."
            chrome_policy {
              AutofillAddressEnabled {
                  policy_options {mode: MANDATORY}
                  AutofillAddressEnabled: false
              }
            }
          }
        )");
    case proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY:
      // TODO(crbug.com/515050857): fill out at-memory traffic annotation
      // details.
      return MISSING_TRAFFIC_ANNOTATION;
    case proto::CONTEXT_MEMORY_FEATURE_UNSPECIFIED:
    default:
      NOTREACHED();
  }
}

void OnAccessTokenRequestCompleted(
    std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>,
    base::OnceCallback<void(std::string_view)> callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(callback).Run(std::string_view());
    return;
  }
  std::move(callback).Run(access_token_info.token);
}

void HandleTokenRequestFlow(
    signin::IdentityManager* identity_manager,
    signin::OAuthConsumerId oauth_consumer_id,
    base::OnceCallback<void(std::string_view)> callback) {
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    std::move(callback).Run(std::string_view());
    return;
  }
  auto access_token_fetcher =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          oauth_consumer_id, identity_manager,
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          signin::ConsentLevel::kSignin);
  auto* access_token_fetcher_ptr = access_token_fetcher.get();
  access_token_fetcher_ptr->Start(
      base::BindOnce(&OnAccessTokenRequestCompleted,
                     std::move(access_token_fetcher), std::move(callback)));
}

}  // namespace

// static
proto::FetchContextRequest PersonalContextFetcher::ToFetchContextRequest(
    proto::ContextMemoryFeature feature,
    const google::protobuf::MessageLite& request_metadata) {
  proto::FetchContextRequest request;
  request.set_feature(feature);
  proto::Any* any_metadata = request.mutable_request_metadata();
  any_metadata->set_type_url(
      base::StrCat({kGoogleAPITypeName, request_metadata.GetTypeName()}));
  request_metadata.SerializeToString(any_metadata->mutable_value());
  return request;
}

PersonalContextFetcher::PersonalContextFetcher(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {}

PersonalContextFetcher::~PersonalContextFetcher() {
  RunErrorCallback(ContextMemoryError::FromExecutionError(
      ContextMemoryError::ExecutionError::kCancelled));
}

void PersonalContextFetcher::RunErrorCallback(ContextMemoryError error) {
  std::visit(absl::Overload{
                 [&](FetchContextResponseCallback& callback) {
                   if (callback) {
                     std::move(callback).Run(base::unexpected(error));
                   }
                 },
                 [&](FetchPiiEntitiesResponseCallback& callback) {
                   if (callback) {
                     std::move(callback).Run(base::unexpected(error));
                   }
                 },
                 [](std::monostate) {},
             },
             callback_);
}

template <typename RequestProto, typename CallbackType>
void PersonalContextFetcher::Fetch(proto::ContextMemoryFeature feature,
                                   const RequestProto& request,
                                   std::string_view rpc_method,
                                   std::optional<base::TimeDelta> timeout,
                                   CallbackType callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only one fetch request can be in progress at a time.
  if (!std::holds_alternative<std::monostate>(callback_)) {
    std::move(callback).Run(
        base::unexpected(ContextMemoryError::FromExecutionError(
            ContextMemoryError::ExecutionError::kGenericFailure)));
    return;
  }

  callback_ = std::move(callback);

  std::string serialized_request;
  request.SerializeToString(&serialized_request);

  GURL endpoint_url(
      base::StrCat({features::kContextMemoryServiceBaseUrl.Get(), rpc_method}));

  HandleTokenRequestFlow(
      identity_manager_, signin::OAuthConsumerId::kContextMemoryService,
      base::BindOnce(&PersonalContextFetcher::OnAccessTokenReceived,
                     weak_ptr_factory_.GetWeakPtr(), feature,
                     std::move(endpoint_url), std::move(serialized_request),
                     timeout));
}

void PersonalContextFetcher::FetchContext(
    proto::ContextMemoryFeature feature,
    const google::protobuf::MessageLite& request_metadata,
    std::optional<base::TimeDelta> timeout,
    FetchContextResponseCallback callback) {
  Fetch(feature, ToFetchContextRequest(feature, request_metadata),
        kFetchContextMethod, timeout, std::move(callback));
}

void PersonalContextFetcher::FetchPiiEntities(
    proto::ContextMemoryFeature feature,
    const proto::FetchPiiEntitiesRequest& request,
    std::optional<base::TimeDelta> timeout,
    FetchPiiEntitiesResponseCallback callback) {
  Fetch(feature, request, kFetchPiiEntitiesMethod, timeout,
        std::move(callback));
}

void PersonalContextFetcher::OnAccessTokenReceived(
    proto::ContextMemoryFeature feature,
    GURL endpoint_url,
    std::string serialized_request,
    std::optional<base::TimeDelta> timeout,
    std::string_view access_token) {
  if (access_token.empty()) {
    RunErrorCallback(ContextMemoryError::FromExecutionError(
        ContextMemoryError::ExecutionError::kPermissionDenied));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = endpoint_url;

  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({kAuthorizationBearerPrefix, access_token}));
  if (timeout && timeout->is_positive()) {
    resource_request->headers.SetHeader(
        kServerTimeoutHeader, base::NumberToString(timeout->InSeconds()));
  }

  resource_request->method = kHttpPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  active_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), GetNetworkTrafficAnnotation(feature));

  active_url_loader_->AttachStringForUpload(std::move(serialized_request),
                                            kRequestContentType);
  active_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PersonalContextFetcher::OnURLLoadComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PersonalContextFetcher::OnURLLoadComplete(
    std::optional<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto net_error = active_url_loader_->NetError();
  int response_code = -1;
  if (active_url_loader_->ResponseInfo() &&
      active_url_loader_->ResponseInfo()->headers) {
    response_code =
        active_url_loader_->ResponseInfo()->headers->response_code();
  }

  active_url_loader_.reset();

  if (net_error != net::OK || response_code != net::HTTP_OK) {
    RunErrorCallback(
        ContextMemoryError::FromHttpStatusCode(static_cast<net::HttpStatusCode>(
            response_code > 0 ? response_code
                              : net::HTTP_INTERNAL_SERVER_ERROR)));
    return;
  }

  std::visit(
      absl::Overload{
          [&](FetchContextResponseCallback& callback) {
            if (!callback) {
              return;
            }
            proto::FetchContextResponse fetch_response;
            if (!response_body ||
                !fetch_response.ParseFromString(*response_body)) {
              std::move(callback).Run(
                  base::unexpected(ContextMemoryError::FromExecutionError(
                      ContextMemoryError::ExecutionError::kGenericFailure)));
              return;
            }
            std::move(callback).Run(base::ok(fetch_response));
          },
          [&](FetchPiiEntitiesResponseCallback& callback) {
            if (!callback) {
              return;
            }
            proto::FetchPiiEntitiesResponse pii_response;
            if (!response_body ||
                !pii_response.ParseFromString(*response_body)) {
              std::move(callback).Run(
                  base::unexpected(ContextMemoryError::FromExecutionError(
                      ContextMemoryError::ExecutionError::kGenericFailure)));
              return;
            }
            std::move(callback).Run(base::ok(pii_response));
          },
          [](std::monostate) {},
      },
      callback_);
}
}  // namespace personal_context
