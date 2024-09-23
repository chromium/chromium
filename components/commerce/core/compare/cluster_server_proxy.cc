// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/cluster_server_proxy.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/compare/compare_utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace commerce {
namespace {
// RPC timeout duration.
const int kTimeoutMs = 5000;

// URL to get compare result.
const char kDefaultServiceBaseUrl[] =
    "https://memex-pa.googleapis.com/v1/shopping/products:related";
const char kBaseUrlParam[] = "cluster_service_base_url";
constexpr base::FeatureParam<std::string> kServiceBaseUrl{
    &commerce::kProductSpecifications, kBaseUrlParam, kDefaultServiceBaseUrl};

// lowerCamelCase JSON proto message keys.
const char kProductIdsKey[] = "productIds";
const char kTypeKey[] = "type";
const char kGPCTypeName[] = "GLOBAL_PRODUCT_CLUSTER_ID";
const char kIdentifierKey[] = "identifier";

constexpr net::NetworkTrafficAnnotationTag
    kGetComparableProductsTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation(
            "chrome_commerce_comparable_products",
            R"(
          semantics {
            sender: "Chrome Shopping"
            description:
              "Ask server for comparable products from a list of product Ids."
            trigger:
              "A Chrome-initiated request that requires user enabling the "
              "Tab Compare feature. The request is sent after Chrome detects "
              "several open tabs has similar products, before showing the user"
              "a button to allow them to compare some of the products."
            user_data {
              type: OTHER
              type: ACCESS_TOKEN
            }
            data:
              "Product cluster IDs obtained previously from the Google server "
              "for product pages user opened."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts { email: "chrome-shopping-eng@google.com" }
            }
            last_reviewed: "2024-06-10"
          }
          policy {
            cookies_allowed: NO
            setting:
              "This fetch is enabled for any user that is eligible to use this "
              "feature based on things like country, locale, and whether the "
              "user is signed in. The request is enabled with the Tab Compare "
              "feature."
            chrome_policy {
              TabCompareSettings {
                policy_options {mode: MANDATORY}
                TabCompareSettings: 2
              }
            }
          }
        )");

// Deserializes a product cluster ID from JSON.
std::optional<uint64_t> Deserialize(const base::Value& value) {
  if (!value.is_dict()) {
    return std::nullopt;
  }

  const base::Value::Dict& value_dict = value.GetDict();
  auto* type = value_dict.FindString(kTypeKey);
  if (!type || *type != kGPCTypeName) {
    return std::nullopt;
  }

  auto* identifier = value_dict.FindString(kIdentifierKey);
  uint64_t product_cluster_id;
  if (!identifier || !base::StringToUint64(*identifier, &product_cluster_id)) {
    return std::nullopt;
  }

  return product_cluster_id;
}

}  // namespace

ClusterServerProxy::ClusterServerProxy(
    signin::IdentityManager* identity_manager,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory) {}

ClusterServerProxy::~ClusterServerProxy() = default;

void ClusterServerProxy::GetComparableProducts(
    const std::vector<uint64_t>& product_cluster_ids,
    GetComparableProductsCallback callback) {
  auto fetcher = CreateEndpointFetcher(
      GURL(kServiceBaseUrl.Get()),
      GetJsonStringForProductClusterIds(product_cluster_ids));
  auto* const fetcher_ptr = fetcher.get();

  fetcher_ptr->Fetch(base::BindOnce(&ClusterServerProxy::HandleCompareResponse,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback), std::move(fetcher)));
}

std::unique_ptr<EndpointFetcher> ClusterServerProxy::CreateEndpointFetcher(
    const GURL& url,
    const std::string& post_data) {
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, kOAuthName, url, kPostHttpMethod, kContentType,
      std::vector<std::string>{kOAuthScope}, base::Milliseconds(kTimeoutMs),
      post_data, kGetComparableProductsTrafficAnnotation, identity_manager_,
      signin::ConsentLevel::kSignin);
}

void ClusterServerProxy::HandleCompareResponse(
    GetComparableProductsCallback callback,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> response) {
  if (response->http_status_code != net::HTTP_OK || response->error_type) {
    VLOG(1) << "Got bad response (" << response->http_status_code
            << ") for comparable products!";
    std::move(callback).Run(std::vector<uint64_t>());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      response->response,
      base::BindOnce(&ClusterServerProxy::OnResponseJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ClusterServerProxy::OnResponseJsonParsed(
    GetComparableProductsCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  std::vector<uint64_t> product_cluster_ids;
  if (result.has_value() && result->is_dict()) {
    if (auto* response_json = result->GetDict().FindList(kProductIdsKey)) {
      for (const auto& product_id_json : *response_json) {
        if (auto product_id = Deserialize(product_id_json)) {
          product_cluster_ids.push_back(*product_id);
        }
      }
    }
  }
  std::move(callback).Run(std::move(product_cluster_ids));
}

}  // namespace commerce
