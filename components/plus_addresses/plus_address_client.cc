// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_client.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "components/plus_addresses/features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace plus_addresses {

namespace {

const base::TimeDelta kRequestTimeout = base::Seconds(5);

// See docs/network_traffic_annotations.md for reference.
// TODO(b/277532955): Update the description and trigger fields when possible.
//                Also replace the policy_exception with a policy if we add one.
const net::NetworkTrafficAnnotationTag kCreatePlusAddressAnnotation =
    net::DefineNetworkTrafficAnnotation("plus_address_creation", R"(
      semantics {
        sender: "Chrome Plus Address Client"
        description: "A plus address is created on the enterprise-specified "
                      "server with this request."
        trigger: "User chooses to create a plus address."
        internal {
          contacts {
              email: "dc-komics@google.com"
          }
        }
        user_data {
          type: ACCESS_TOKEN,
          type: SENSITIVE_URL
        }
        data: "The site on which the user wants to use a plus address is sent."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2023-09-07"
      }
      policy {
        cookies_allowed: NO
        setting: "Disable the Plus Addresses feature."
        policy_exception_justification: "We don't have an opt-out policy yet"
                                        " as Plus Addresses hasn't launched."
      }
    )");

absl::optional<GURL> ValidateAndGetUrl() {
  GURL maybe_url = GURL(kEnterprisePlusAddressServerUrl.Get());
  return maybe_url.is_valid() ? absl::make_optional(maybe_url) : absl::nullopt;
}

//   If `response` is populated, it will be in JSON and fit this schema:
//   {
//     "plusProfile": [
//       {
//         "plusEmail": {
//           "address": string,
//         },
//       }
//     ]
//   }
//  This method parses out the relevant plus address from this JSON value.
absl::optional<std::string> ParsePlusAddressFromV1CreateResponse(
    data_decoder::DataDecoder::ValueOrError response) {
  if (!response.has_value() || !response->is_dict()) {
    return absl::nullopt;
  }

  // Use iterators to avoid looking up by JSON keys.
  for (const std::pair<const std::string&, const base::Value&>
           first_level_entry : response.value().GetDict()) {
    const auto& [first_key, first_val] = first_level_entry;
    if (!base::MatchPattern(first_key, "*Profile") || !first_val.is_list()) {
      continue;
    }
    const base::Value::List* profile_list = &first_val.GetList();

    // Note: assumes server will only return 1 profile. This is what we
    // expect in v1.
    if (profile_list->size() != 1 || !(*profile_list)[0].is_dict()) {
      return absl::nullopt;
    }

    // Note: This has many entries, but we only want the dict-type one.
    // Use the iterator to avoid looking up by "PlusAddressWrapper" key.
    for (const std::pair<const std::string&, const base::Value&>
             second_level_entry : (*profile_list)[0].GetDict()) {
      const auto& [second_key, second_val] = second_level_entry;
      if (!base::MatchPattern(second_key, "*Email") || !second_val.is_dict()) {
        continue;
      }

      const base::Value::Dict* email = &second_val.GetDict();

      for (const std::pair<const std::string&, const base::Value&>
               third_level_entry : *email) {
        const auto& [third_key, third_val] = third_level_entry;
        if (base::MatchPattern(third_key, "*Address") &&
            third_val.is_string()) {
          return absl::make_optional(third_val.GetString());
        }
      }
    }
  }
  return absl::nullopt;
}

}  // namespace

PlusAddressClient::PlusAddressClient(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)),
      auth_token_provider_(identity_manager,
                           {kEnterprisePlusAddressOAuthScope.Get()}),
      server_url_(ValidateAndGetUrl()) {}

PlusAddressClient::~PlusAddressClient() = default;

void PlusAddressClient::CreatePlusAddress(const std::string& site,
                                          PlusAddressCallback callback) {
  if (!server_url_) {
    return;
  }

  auth_token_provider_.GetAuthToken(
      base::BindOnce(&PlusAddressClient::CreatePlusAddressWithToken,
                     // base::Unretained is safe here since this owns the
                     // auth_token_provider_ and they have the same lifetime.
                     base::Unretained(this), site, std::move(callback)));
}

// static
absl::optional<std::string>
PlusAddressClient::ParsePlusAddressFromV1CreateForTesting(
    data_decoder::DataDecoder::ValueOrError response) {
  return ParsePlusAddressFromV1CreateResponse(std::move(response));
}

void PlusAddressClient::CreatePlusAddressWithToken(const std::string& site,
                                                   PlusAddressCallback callback,
                                                   const std::string& token) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = net::HttpRequestHeaders::kPutMethod;
  resource_request->url =
      server_url_.value().Resolve(kServerPlusProfileEndpoint);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      base::StrCat({"Bearer ", token}));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  base::Value::Dict payload;
  payload.Set("facet", site);
  std::string request_body;
  bool wrote_payload = base::JSONWriter::Write(payload, &request_body);
  DCHECK(wrote_payload);

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kCreatePlusAddressAnnotation);
  url_loader_->AttachStringForUpload(request_body, "application/json");

  url_loader_->SetTimeoutDuration(kRequestTimeout);
  // Use max download size for now.
  // TODO(kaklilu) - Measure average downloadsize and pick a more appropriate
  // one.
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          [](PlusAddressCallback callback,
             std::unique_ptr<std::string> response) {
            if (!response) {
              // The request has failed.
              // TODO: Add metrics here.
              return;
            }
            data_decoder::DataDecoder::ParseJsonIsolated(
                *response, base::BindOnce(&ParsePlusAddressFromV1CreateResponse)
                               .Then(base::BindOnce(
                                   [](PlusAddressCallback callback,
                                      absl::optional<std::string> result) {
                                     if (result.has_value()) {
                                       std::move(callback).Run(result.value());
                                     }
                                   },
                                   std::move(callback))));
          },
          std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

}  // namespace plus_addresses
