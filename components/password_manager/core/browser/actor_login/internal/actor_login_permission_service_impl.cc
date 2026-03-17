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
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace actor_login {

namespace {

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
    const FederatedOrigins& origins) {
  base::DictValue federated_filter;
  if (!origins.embedder_origin.opaque()) {
    federated_filter.Set("rpEmbedderOrigin",
                         origins.embedder_origin.Serialize());
  }
  if (!origins.requester_origin.opaque()) {
    federated_filter.Set("rpRequesterOrigin",
                         origins.requester_origin.Serialize());
  }
  federated_filter.Set("matchAffiliatedRequesterOrigins", true);

  return base::DictValue().Set("federatedCredentialPermissionFilter",
                               std::move(federated_filter));
}

std::string CreateDeleteRequestBody(const url::Origin& embedder_origin) {
  base::ListValue filters;
  filters.Append(
      CreateFederatedPermissionFilter({.embedder_origin = embedder_origin}));
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
  Request(const GURL& url,
          const std::string& post_data,
          scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
          base::OnceCallback<void(Request*, std::optional<std::string>)>
              completion_callback);
  ~Request();

  void Start();

  bool success() const;

 private:
  void OnSimpleLoaderComplete(std::optional<std::string> response_body);

  GURL url_;
  std::string post_data_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::OnceCallback<void(Request*, std::optional<std::string>)>
      completion_callback_;
};

ActorLoginPermissionServiceImpl::Request::Request(
    const GURL& url,
    const std::string& post_data,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::OnceCallback<void(Request*, std::optional<std::string>)>
        completion_callback)
    : url_(url),
      post_data_(post_data),
      url_loader_factory_(std::move(url_loader_factory)),
      completion_callback_(std::move(completion_callback)) {}

ActorLoginPermissionServiceImpl::Request::~Request() = default;

void ActorLoginPermissionServiceImpl::Request::Start() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // TODO(crbug.com/491049268): Fetch and attach OAuth2 token using
  // IdentityManager.

  loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kActorLoginPermissionTrafficAnnotation);
  loader_->AttachStringForUpload(post_data_, "application/json");
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
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

ActorLoginPermissionServiceImpl::~ActorLoginPermissionServiceImpl() = default;

void ActorLoginPermissionServiceImpl::ListAllPermissions(
    ListPermissionsResult callback) {
  GURL url(base::StrCat({kActorLoginPermissionServiceUrlBase, "list"}));
  std::string post_data = CreateListRequestBody({});

  StartRequest(std::make_unique<Request>(
      url, post_data, url_loader_factory_,
      base::BindOnce(&ActorLoginPermissionServiceImpl::OnListRequestCompleted,
                     base::Unretained(this))
          .Then(std::move(callback))));
}

void ActorLoginPermissionServiceImpl::DeletePermission(
    const url::Origin& embedder_origin,
    DeletePermissionResult callback) {
  if (embedder_origin.opaque()) {
    std::move(callback).Run(false);
    return;
  }

  GURL url(base::StrCat({kActorLoginPermissionServiceUrlBase, "delete"}));
  std::string post_data = CreateDeleteRequestBody(embedder_origin);

  StartRequest(std::make_unique<Request>(
      url, post_data, url_loader_factory_,
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
      url, post_data, url_loader_factory_,
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
