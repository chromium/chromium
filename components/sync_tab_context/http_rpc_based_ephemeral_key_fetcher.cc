// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/http_rpc_based_ephemeral_key_fetcher.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/protocol/agile_encryption_keys.pb.h"
#include "components/sync_tab_context/proto/ephemeral_key_service.pb.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace sync_tab_context {

namespace {
// Maximum response size for ephemeral key response (1 MB).
constexpr size_t kMaxResponseSizeBytes = 1024 * 1024;
}  // namespace

class HttpRpcBasedEphemeralKeyFetcher::Operation {
 public:
  using CompletionCallback = base::OnceCallback<void(std::optional<Result>)>;

  Operation(signin::IdentityManager* identity_manager,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
            const GURL& server_url)
      : identity_manager_(identity_manager),
        url_loader_factory_(std::move(url_loader_factory)),
        server_url_(server_url) {}

  ~Operation() = default;

  void Start(CompletionCallback completion_callback) {
    completion_callback_ = std::move(completion_callback);

    // `base::Unretained(this)` is safe because `access_token_fetcher_` is owned
    // by `this`, so destroying `Operation` destroys `access_token_fetcher_`
    // and cancels its callback.
    access_token_fetcher_ =
        std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
            signin::OAuthConsumerId::kTabContextContainersService,
            identity_manager_,
            base::BindOnce(&Operation::OnAccessTokenFetched,
                           base::Unretained(this)),
            signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
            signin::ConsentLevel::kSignin);
  }

 private:
  static std::optional<Result> BuildResultFromResponse(
      std::optional<std::string> response_body) {
    if (!response_body) {
      return std::nullopt;
    }

    GenerateEphemeralKeyResponse response_proto;
    if (!response_proto.ParseFromString(*response_body)) {
      return std::nullopt;
    }

    if (response_proto.server_token().empty() ||
        !response_proto.has_agile_symmetric_key_set()) {
      return std::nullopt;
    }

    auto key_set = syncer::AgileSymmetricKeySet::FromProto(
        response_proto.agile_symmetric_key_set());
    if (!key_set || key_set->size() == 0 || key_set->primary_key_id() == 0) {
      return std::nullopt;
    }

    return Result{.ephemeral_key = std::move(key_set),
                  .server_token = response_proto.server_token()};
  }

  void OnAccessTokenFetched(GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info) {
    if (error.state() != GoogleServiceAuthError::NONE) {
      CHECK(completion_callback_);
      std::move(completion_callback_).Run(std::nullopt);
      return;
    }

    GenerateEphemeralKeyRequest request_proto;
    const std::string request_body = request_proto.SerializeAsString();

    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = server_url_;
    resource_request->method = "POST";
    resource_request->load_flags =
        net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
    resource_request->credentials_mode =
        google_apis::GetOmitCredentialsModeForGaiaRequests();
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        base::StrCat({"Bearer ", access_token_info.token}));

    const net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("tab_context_ephemeral_key_fetch",
                                            R"(
          semantics {
            sender: "Tab Context Sync Service"
            description:
              "Fetches an ephemeral encryption key to provide access to "
              "tab context data (some unspecified representation of tab "
              "content) stored in encrypted form server-side."
            trigger: "Tab context container access request."
            data:
              "An OAuth2 access token sent in the HTTP Authorization header to "
              "authenticate the request. The request body is empty."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts {
                email: "//components/sync_tab_context/OWNERS"
              }
            }
            user_data {
              type: ACCESS_TOKEN
            }
            last_reviewed: "2026-07-17"
          }
          policy {
            cookies_allowed: NO
            setting: "Users can disable Sync in Chrome settings."
            chrome_policy {
              SyncDisabled {
                SyncDisabled: true
              }
            }
          })");

    // `base::Unretained(this)` is safe because `url_loader_` is owned by
    // `this`, so destroying `Operation` destroys `url_loader_` and cancels its
    // callback.
    url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                   traffic_annotation);
    url_loader_->AttachStringForUpload(request_body, "application/x-protobuf");
    url_loader_->DownloadToString(
        url_loader_factory_.get(),
        base::BindOnce(&Operation::OnSimpleURLLoaderComplete,
                       base::Unretained(this)),
        kMaxResponseSizeBytes);
  }

  void OnSimpleURLLoaderComplete(std::optional<std::string> response_body) {
    CHECK(completion_callback_);
    std::move(completion_callback_)
        .Run(BuildResultFromResponse(std::move(response_body)));
  }

  const raw_ptr<signin::IdentityManager> identity_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const GURL server_url_;

  CompletionCallback completion_callback_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

HttpRpcBasedEphemeralKeyFetcher::HttpRpcBasedEphemeralKeyFetcher(
    signin::IdentityManager* identity_manager,
    UrlLoaderFactoryGetter url_loader_factory_getter,
    const GURL& server_url)
    : identity_manager_(identity_manager),
      url_loader_factory_getter_(std::move(url_loader_factory_getter)),
      server_url_(server_url) {
  CHECK(identity_manager_);
  CHECK(url_loader_factory_getter_);
}

HttpRpcBasedEphemeralKeyFetcher::~HttpRpcBasedEphemeralKeyFetcher() = default;

void HttpRpcBasedEphemeralKeyFetcher::FetchEphemeralKey(
    FetchCallback callback) {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      url_loader_factory_getter_.Run();
  if (!server_url_.is_valid() || !url_loader_factory) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  auto op = std::make_unique<Operation>(
      identity_manager_, std::move(url_loader_factory), server_url_);
  Operation* op_ptr = op.get();
  ongoing_operations_.push_back(std::move(op));

  // `base::Unretained(this)` is safe because `this` owns `ongoing_operations_`.
  // Destroying `HttpRpcBasedEphemeralKeyFetcher` deletes `ongoing_operations_`,
  // canceling all in-flight operations and ensuring this callback is never
  // invoked after `this` is destroyed.
  op_ptr->Start(
      base::BindOnce(&HttpRpcBasedEphemeralKeyFetcher::OnOperationCompleted,
                     base::Unretained(this), op_ptr, std::move(callback)));
}

void HttpRpcBasedEphemeralKeyFetcher::OnOperationCompleted(
    Operation* op,
    FetchCallback callback,
    std::optional<Result> result) {
  std::erase_if(ongoing_operations_,
                [op](const auto& item) { return item.get() == op; });
  std::move(callback).Run(std::move(result));
}

}  // namespace sync_tab_context
