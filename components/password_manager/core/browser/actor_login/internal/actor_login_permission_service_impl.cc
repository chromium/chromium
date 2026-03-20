// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_permission_service_impl.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace actor_login {

namespace {
// The maximum number of retries for the `SimpleURLLoader` requests.
const size_t kMaxRetries = 1;

// TODO(crbug.com/491035927): Update to prod URL when available.
const char kActorLoginPermissionServiceUrlBase[] =
    "https://staging-agenticpermission.pa.sandbox.googleapis.com/v1/"
    "permissions:";

constexpr net::NetworkTrafficAnnotationTag
    kActorLoginPermissionTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("actor_login_permission_service",
                                            R"(
      semantics {
        sender: "Actor Login Permission Service"
        description:
          "Communicates with Agentic permission service to list, grant and "
          "revoke permissions for actor login using Sign in with Google."
        trigger:
          "Triggered when a user uses Chrome's agent to navigate the web, "
          "specifically when the agent decides it needs to log in to a "
          "website. Alternatively, the traffic is triggered from the Chrome "
          "settings page that manages permissions that were already granted."
        data:
          "All requests will contain OAuth2 token of the primary Chrome "
          "profile. On top of that, the requests will contain: "
          " - List and delete: embedder and requester origins. Embedder origin "
          "is the main frame origin of the website where the permission was "
          "granted. Requester origin is the origin of the frame that sent "
          "requests to the IdP to authenticate the user. In most cases these "
          "are the same but it's not guaranteed."
          " - Grant: embedder and requester origins, email, user ID, IdP "
          "origin (e.g. accounts.google.com)."
          "List response will contain: "
          " - IdP origin"
          " - embedder and requester origins"
          " - email"
          " - user ID"
        user_data {
          type: ACCESS_TOKEN
          type: SENSITIVE_URL
          type: USERNAME
        }
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "mainpass-team@google.com"
          }
        }
        last_reviewed: "2026-03-13"
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can disable this feature by turning off the Let Chrome browse "
          "for you settings under chrome://settings/ai/gemini. Traffic from "
          "the management UI will not be disabled because it's important for "
          "users to be able to delete their permissions. Deleting all "
          "permsissions will stop the traffic."
        chrome_policy {
          GeminiActOnWebSettings {
            GeminiActOnWebSettings: 0
          }
        }
      })");

base::DictValue CreateFederatedPermissionFilter(
    const FederatedOrigins& origins,
    const std::string& display_name) {
  base::DictValue federated_filter;
  if (!origins.embedder_origin.opaque()) {
    federated_filter.Set("rpEmbedderOrigin",
                         origins.embedder_origin.Serialize());
  }
  if (!origins.requester_origin.opaque()) {
    federated_filter.Set("rpRequesterOrigin",
                         origins.requester_origin.Serialize());
  }
  if (!display_name.empty()) {
    federated_filter.Set("chosenAccountEmail", display_name);
  }
  federated_filter.Set("matchAffiliatedRequesterOrigins", true);

  return base::DictValue().Set("federatedCredentialPermissionFilter",
                               std::move(federated_filter));
}

base::DictValue CreateFederatedPermissionFilter(
    const FederatedOrigins& origins) {
  return CreateFederatedPermissionFilter(origins, /*display_name=*/"");
}

std::string CreateDeleteRequestBody(const url::Origin& embedder_origin,
                                    const std::string& display_name) {
  base::ListValue filters;
  filters.Append(CreateFederatedPermissionFilter(
      {.embedder_origin = embedder_origin}, display_name));
  auto request_dict = base::DictValue().Set("filter", std::move(filters));

  std::string post_data;
  base::JSONWriter::Write(request_dict, &post_data);
  return post_data;
}

std::string CreateGrantRequestBody(const FederatedPermission& permission) {
  // All fields in `permission` are required to be meaningful except for
  // `affiliated_requester_origins`.
  CHECK(!permission.idp_origin.opaque());
  CHECK(!permission.rp_embedder_origin.opaque());
  CHECK(!permission.rp_requester_origin.opaque());
  CHECK(!permission.chosen_account_id.empty());
  CHECK(!permission.chosen_account_email.empty());

  auto federated_permission_dict =
      base::DictValue()
          .Set("idpOrigin", permission.idp_origin.Serialize())
          .Set("rpEmbedderOrigin", permission.rp_embedder_origin.Serialize())
          .Set("rpRequesterOrigin", permission.rp_requester_origin.Serialize())
          .Set("chosenAccountId", permission.chosen_account_id);
  auto request_dict = base::DictValue().Set(
      "federatedCredentialPermission", std::move(federated_permission_dict));

  std::string post_data;
  base::JSONWriter::Write(request_dict, &post_data);
  return post_data;
}

std::string CreateListRequestBody(
    const std::vector<FederatedOrigins>& origins) {
  base::ListValue filters;
  for (const auto& origin : origins) {
    filters.Append(CreateFederatedPermissionFilter(origin));
  }
  auto request_dict = base::DictValue().Set("filters", std::move(filters));

  std::string post_data;
  base::JSONWriter::Write(request_dict, &post_data);
  return post_data;
}

FederatedPermission ParseFederatedPermission(const base::DictValue& dict) {
  FederatedPermission permission;

  if (const std::string* idp_origin = dict.FindString("idpOrigin")) {
    permission.idp_origin = url::Origin::Create(GURL(*idp_origin));
  }

  if (const std::string* rp_embedder_origin =
          dict.FindString("rpEmbedderOrigin")) {
    permission.rp_embedder_origin =
        url::Origin::Create(GURL(*rp_embedder_origin));
  }

  if (const std::string* rp_requester_origin =
          dict.FindString("rpRequesterOrigin")) {
    permission.rp_requester_origin =
        url::Origin::Create(GURL(*rp_requester_origin));
  }

  if (const std::string* chosen_account_id =
          dict.FindString("chosenAccountId")) {
    permission.chosen_account_id = *chosen_account_id;
  }

  if (const std::string* chosen_account_email =
          dict.FindString("chosenAccountEmail")) {
    permission.chosen_account_email = *chosen_account_email;
  }

  if (const base::ListValue* affiliated_requester_origins =
          dict.FindList("affiliatedRequesterOrigins")) {
    for (const base::Value& value : *affiliated_requester_origins) {
      if (const std::string* origin = value.GetIfString()) {
        permission.affiliated_requester_origins.push_back(*origin);
      }
    }
  }

  return permission;
}

std::vector<FederatedPermission> ParseFederatedPermissionsList(
    const base::DictValue& response) {
  std::vector<FederatedPermission> parsed_permissions;
  const base::ListValue* permissions_list = response.FindList("permissions");
  if (!permissions_list) {
    return {};
  }

  for (const base::Value& permission_value : *permissions_list) {
    const base::DictValue* permission_dict = permission_value.GetIfDict();
    if (!permission_dict) {
      continue;
    }

    if (const base::DictValue* federated_permission_dict =
            permission_dict->FindDict("federatedCredentialPermission")) {
      parsed_permissions.push_back(
          ParseFederatedPermission(*federated_permission_dict));
    }
  }
  return parsed_permissions;
}

}  // namespace

class ActorLoginPermissionServiceImpl::Request {
 public:
  Request(signin::IdentityManager* identity_manager,
          const GURL& url,
          const std::string& post_data,
          scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
          base::OnceCallback<void(Request*, std::optional<std::string>)>
              completion_callback);
  ~Request();

  void Start();

  bool success() const;

 private:
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);
  void OnSimpleLoaderComplete(std::optional<std::string> response_body);

  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  GURL url_;
  std::string post_data_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::OnceCallback<void(Request*, std::optional<std::string>)>
      completion_callback_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
};

ActorLoginPermissionServiceImpl::Request::Request(
    signin::IdentityManager* identity_manager,
    const GURL& url,
    const std::string& post_data,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::OnceCallback<void(Request*, std::optional<std::string>)>
        completion_callback)
    : identity_manager_(identity_manager),
      url_(url),
      post_data_(post_data),
      url_loader_factory_(std::move(url_loader_factory)),
      completion_callback_(std::move(completion_callback)) {}

ActorLoginPermissionServiceImpl::Request::~Request() = default;

void ActorLoginPermissionServiceImpl::Request::Start() {
  if (!identity_manager_) {
    // If there is no `IdentityManager`, we cannot fetch an access token.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(completion_callback_), this, std::nullopt));
    return;
  }

  access_token_fetcher_ = std::make_unique<
      signin::PrimaryAccountAccessTokenFetcher>(
      signin::OAuthConsumerId::kActorLoginPermissionService, identity_manager_,
      base::BindOnce(
          &ActorLoginPermissionServiceImpl::Request::OnAccessTokenFetchComplete,
          base::Unretained(this)),
      // Send the request immediately. Otherwise we can wait forever if the
      // user is not signed in.
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSignin);
}

void ActorLoginPermissionServiceImpl::Request::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(completion_callback_).Run(this, std::nullopt);
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + access_token_info.token);
  resource_request->headers.SetHeader(
      "X-Developer-Key", GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      "application/json");

  loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kActorLoginPermissionTrafficAnnotation);
  loader_->AttachStringForUpload(post_data_, "application/json");
  loader_->SetRetryOptions(kMaxRetries, network::SimpleURLLoader::RETRY_ON_5XX);
  loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(
          &ActorLoginPermissionServiceImpl::Request::OnSimpleLoaderComplete,
          base::Unretained(this)));
}

void ActorLoginPermissionServiceImpl::Request::OnSimpleLoaderComplete(
    std::optional<std::string> response_body) {
  // The request is destroyed as part of this call.
  std::move(completion_callback_).Run(this, std::move(response_body));
}

bool ActorLoginPermissionServiceImpl::Request::success() const {
  return loader_ && loader_->NetError() == net::OK;
}

ActorLoginPermissionServiceImpl::ActorLoginPermissionServiceImpl(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {}

ActorLoginPermissionServiceImpl::~ActorLoginPermissionServiceImpl() = default;

void ActorLoginPermissionServiceImpl::ListPermissions(
    const std::vector<FederatedOrigins>& origins,
    ListPermissionsResult callback) {
  GURL url(base::StrCat({kActorLoginPermissionServiceUrlBase, "list"}));
  std::string post_data = CreateListRequestBody(origins);

  StartRequest(std::make_unique<Request>(
      identity_manager_, url, post_data, url_loader_factory_,
      base::BindOnce(&ActorLoginPermissionServiceImpl::OnListRequestCompleted,
                     base::Unretained(this))
          .Then(std::move(callback))));
}

void ActorLoginPermissionServiceImpl::ListAllPermissions(
    ListPermissionsResult callback) {
  ListPermissions({}, std::move(callback));
}

void ActorLoginPermissionServiceImpl::DeletePermission(
    const url::Origin& embedder_origin,
    const std::string& display_name,
    DeletePermissionResult callback) {
  if (embedder_origin.opaque()) {
    std::move(callback).Run(false);
    return;
  }

  GURL url(base::StrCat({kActorLoginPermissionServiceUrlBase, "delete"}));
  std::string post_data =
      CreateDeleteRequestBody(embedder_origin, display_name);

  StartRequest(std::make_unique<Request>(
      identity_manager_, url, post_data, url_loader_factory_,
      base::BindOnce(
          &ActorLoginPermissionServiceImpl::OnGenericRequestCompleted,
          base::Unretained(this))
          .Then(std::move(callback))));
}

void ActorLoginPermissionServiceImpl::GrantPermission(
    const FederatedPermission& permission,
    GrantPermissionResult callback) {
  GURL url(base::StrCat(
      // allow_missing means that if the permission does not exist, it will be
      // created. This effectively means that the request becomes a "Grant or
      // update" request.
      {kActorLoginPermissionServiceUrlBase, "update?allow_missing=true"}));
  std::string post_data = CreateGrantRequestBody(permission);

  StartRequest(std::make_unique<Request>(
      identity_manager_, url, post_data, url_loader_factory_,
      base::BindOnce(
          &ActorLoginPermissionServiceImpl::OnGenericRequestCompleted,
          base::Unretained(this))
          .Then(std::move(callback))));
}

void ActorLoginPermissionServiceImpl::StartRequest(
    std::unique_ptr<Request> request) {
  Request* request_ptr = request.get();
  pending_requests_.push_back(std::move(request));
  request_ptr->Start();
}

std::vector<FederatedPermission>
ActorLoginPermissionServiceImpl::OnListRequestCompleted(
    Request* request,
    std::optional<std::string> response_body) {
  std::erase_if(pending_requests_,
                [&](const std::unique_ptr<Request>& pending_request) {
                  return pending_request.get() == request;
                });

  if (!response_body) {
    return {};
  }

  std::optional<base::DictValue> response = base::JSONReader::ReadDict(
      *response_body, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!response) {
    return {};
  }

  return ParseFederatedPermissionsList(*response);
}

bool ActorLoginPermissionServiceImpl::OnGenericRequestCompleted(
    Request* request,
    std::optional<std::string> response_body) {
  bool success = request->success();
  std::erase_if(pending_requests_,
                [&](const std::unique_ptr<Request>& pending_request) {
                  return pending_request.get() == request;
                });

  return success;
}

}  // namespace actor_login
