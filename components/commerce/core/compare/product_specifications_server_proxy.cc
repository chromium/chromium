// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/product_specifications_server_proxy.h"

#include <optional>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/feature_utils.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/json_sanitizer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace commerce {

namespace {
const char kProductSpecificationsKey[] = "productSpecifications";
const char kProductSpecificationSectionsKey[] = "productSpecificationSections";
const char kProductSpecificationValuesKey[] = "productSpecificationValues";
const char kProductIdsKey[] = "productIds";
const char kKeyKey[] = "key";
const char kTitleKey[] = "title";
const char kTypeKey[] = "type";
const char kIdentifierKey[] = "identifier";
const char kIdentifiersKey[] = "identifiers";
const char kGPCKey[] = "gpcId";
const char kMIDKey[] = "mid";
const char kImageURLKey[] = "imageUrl";
const char kDescriptionsKey[] = "descriptions";
const char kSummaryKey[] = "summary";

const char kGPCTypeName[] = "GLOBAL_PRODUCT_CLUSTER_ID";

const uint64_t kTimeoutMs = 5000;

constexpr net::NetworkTrafficAnnotationTag kShoppingListTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("product_specifications_fetcher",
                                        R"(
        semantics {
          sender: "Chrome Shopping"
          description:
            "Retrieves product specifications for a list of products as they "
            "relate to each other based on their cluster IDs. This will only "
            "be called while the UI for the feature is open."
          trigger:
            "When the product specifications UI is opened, we will send a "
            "request any time the list of currently viewed products changes."
          user_data {
            type: ACCESS_TOKEN
            type: SENSITIVE_URL
          }
          data: "The URL of the page the user is on and an access token based "
            "on the signed in account."
          internal {
            contacts {
                email: "chrome-shopping@google.com"
            }
          }
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2024-02-14"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This fetch is enabled for any user that is eligible to use this "
            "feature based on things like country, locale, and whether the "
            "user is signed in. The request is only made after the user "
            "chooses to engage with the feature."
          chrome_policy {}
        })");

}  // namespace

ProductSpecificationsServerProxy::ProductSpecificationsServerProxy(
    AccountChecker* account_checker,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : account_checker_(account_checker),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {}

ProductSpecificationsServerProxy::~ProductSpecificationsServerProxy() = default;

void ProductSpecificationsServerProxy::GetProductSpecificationsForClusterIds(
    std::vector<uint64_t> cluster_ids,
    ProductSpecificationsCallback callback) {
  if (!IsProductSpecificationsEnabled(account_checker_)) {
    std::move(callback).Run(cluster_ids, std::nullopt);
    return;
  }

  base::Value::List product_id_list;
  for (uint64_t id : cluster_ids) {
    base::Value::Dict id_definition;
    id_definition.Set(kTypeKey, kGPCTypeName);
    id_definition.Set(kIdentifierKey, base::NumberToString(id));
    product_id_list.Append(std::move(id_definition));
  }

  base::Value::Dict json_dict;
  json_dict.Set(kProductIdsKey, std::move(product_id_list));
  std::string post_data;
  base::JSONWriter::Write(json_dict, &post_data);

  auto fetcher = CreateEndpointFetcher(
      GURL(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kProductSpecificationsUrlKey)),
      kPostHttpMethod, post_data);

  auto* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(
      &ProductSpecificationsServerProxy::HandleSpecificationsResponse,
      weak_factory_.GetWeakPtr(), std::move(cluster_ids), std::move(callback),
      std::move(fetcher)));
}

void ProductSpecificationsServerProxy::HandleSpecificationsResponse(
    std::vector<uint64_t> cluster_ids,
    base::OnceCallback<void(std::vector<uint64_t>,
                            std::optional<ProductSpecifications>)> callback,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> responses) {
  if (responses->http_status_code != net::HTTP_OK || responses->error_type) {
    VLOG(1) << "Got bad response (" << responses->http_status_code
            << ") for product specifications!";
    std::move(callback).Run(std::move(cluster_ids), std::nullopt);
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      responses->response,
      base::BindOnce(
          [](base::WeakPtr<ProductSpecificationsServerProxy> proxy,
             std::vector<uint64_t> cluster_ids,
             base::OnceCallback<void(std::vector<uint64_t>,
                                     std::optional<ProductSpecifications>)>
                 callback,
             data_decoder::DataDecoder::ValueOrError result) {
            if (!proxy) {
              std::move(callback).Run(std::move(cluster_ids), std::nullopt);
              return;
            }

            if (!result.has_value() || !result->is_dict()) {
              VLOG(1) << "Failed to parse product specifications JSON!";
              std::move(callback).Run(std::move(cluster_ids), std::nullopt);
              return;
            }

            std::move(callback).Run(
                std::move(cluster_ids),
                ProductSpecificationsFromJsonResponse(result.value()));
          },
          weak_factory_.GetWeakPtr(), std::move(cluster_ids),
          std::move(callback)));
}

std::unique_ptr<EndpointFetcher>
ProductSpecificationsServerProxy::CreateEndpointFetcher(
    const GURL& url,
    const std::string& http_method,
    const std::string& post_data) {
  signin::ConsentLevel consent_level = signin::ConsentLevel::kSignin;
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, kOAuthName, url, http_method, kContentType,
      std::vector<std::string>{kOAuthScope}, base::Milliseconds(kTimeoutMs),
      post_data, kShoppingListTrafficAnnotation, identity_manager_,
      consent_level);
}

std::optional<ProductSpecifications>
ProductSpecificationsServerProxy::ProductSpecificationsFromJsonResponse(
    const base::Value& compareJson) {
  if (!compareJson.is_dict()) {
    return std::nullopt;
  }

  std::optional<ProductSpecifications> product_specs;
  product_specs.emplace();

  const base::Value::Dict* product_specs_dict =
      compareJson.GetDict().FindDict(kProductSpecificationsKey);
  if (!product_specs_dict) {
    return std::nullopt;
  }

  const base::Value::List* spec_sections =
      product_specs_dict->FindList(kProductSpecificationSectionsKey);
  if (!spec_sections) {
    return std::nullopt;
  }

  // Extract the dimensions that the products will be compared by.
  for (const base::Value& section : *spec_sections) {
    if (!section.is_dict()) {
      continue;
    }

    uint64_t section_id;
    const std::string* key_value = section.GetDict().FindString(kKeyKey);
    if (key_value && base::StringToUint64(*key_value, &section_id)) {
      const std::string* title_value = section.GetDict().FindString(kTitleKey);
      product_specs->product_dimension_map[section_id] = *title_value;
    }
  }

  const base::Value::List* specifications =
      product_specs_dict->FindList(kProductSpecificationsKey);
  if (!specifications) {
    return std::nullopt;
  }

  // Extract the individual products that are being compared.
  for (const base::Value& spec : *specifications) {
    ProductSpecifications::Product product;

    if (!spec.is_dict()) {
      continue;
    }

    const base::Value::Dict* id_map = spec.GetDict().FindDict(kIdentifiersKey);
    if (!id_map) {
      continue;
    }

    const std::string* mid = id_map->FindString(kMIDKey);
    if (mid) {
      product.mid = *mid;
    }

    uint64_t cluster_id;
    const std::string* cluster_id_string = id_map->FindString(kGPCKey);
    if (cluster_id_string &&
        base::StringToUint64(*cluster_id_string, &cluster_id)) {
      product.product_cluster_id = cluster_id;
    }

    const std::string* title = spec.GetDict().FindString(kTitleKey);
    if (title) {
      product.title = *title;
    }

    const std::string* image_url = spec.GetDict().FindString(kImageURLKey);
    if (image_url) {
      product.image_url = GURL(*image_url);
    }

    const std::string* summary = spec.GetDict().FindString(kSummaryKey);
    if (summary) {
      product.summary = *summary;
    }

    const base::Value::List* product_spec_values =
        spec.GetDict().FindList(kProductSpecificationValuesKey);
    if (!product_spec_values) {
      continue;
    }

    // Extract the dimensions that the products are being compared by.
    for (const base::Value& spec_value : *product_spec_values) {
      if (!spec_value.is_dict()) {
        continue;
      }

      const std::string* value_id_string =
          spec_value.GetDict().FindString(kKeyKey);
      uint64_t value_id;
      if (!base::StringToUint64(*value_id_string, &value_id)) {
        continue;
      }

      const base::Value::List* descriptions_list =
          spec_value.GetDict().FindList(kDescriptionsKey);
      if (!descriptions_list) {
        continue;
      }

      std::vector<std::string> descriptions;
      for (const base::Value& description : *descriptions_list) {
        descriptions.push_back(description.GetString());
      }

      product.product_dimension_values[value_id] = descriptions;
    }

    product_specs->products.push_back(product);
  }

  return product_specs;
}

}  // namespace commerce
