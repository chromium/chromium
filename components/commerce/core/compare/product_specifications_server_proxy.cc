// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/product_specifications_server_proxy.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/compare/compare_utils.h"
#include "components/commerce/core/feature_utils.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/json_sanitizer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace commerce {

namespace {

const char kEndpointUrl[] =
    "https://memex-pa.googleapis.com/v1/shopping/products:specifications";

const char kAltTextKey[] = "alternativeText";
const char kBuyingOptionsURLKey[] = "buyingOptionsUrl";
const char kDescriptionKey[] = "description";
const char kFaviconUrlKey[] = "faviconUrl";
const char kGPCKey[] = "gpcId";
const char kIdentifiersKey[] = "identifiers";
const char kImageURLKey[] = "imageUrl";
const char kKeyKey[] = "key";
const char kLabelKey[] = "label";
const char kMIDKey[] = "mid";
const char kOptionsKey[] = "options";
const char kProductSpecificationsKey[] = "productSpecifications";
const char kProductSpecificationSectionsKey[] = "productSpecificationSections";
const char kProductSpecificationValuesKey[] = "productSpecificationValues";
const char kSpecificationDescriptionsKey[] = "specificationDescriptions";
const char kSummaryKey[] = "summaryDescription";
const char kThumbnailUrlKey[] = "thumbnailImageUrl";
const char kTitleKey[] = "title";
const char kTextKey[] = "text";
const char kUrlKey[] = "url";
const char kUrlsKey[] = "urls";

const uint64_t kTimeoutMs = 5000;

constexpr net::NetworkTrafficAnnotationTag kShoppingListTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("product_specifications_fetcher",
                                        R"(
        semantics {
          sender: "Chrome Shopping"
          description:
            "Retrieves Tab Comparison data for a list of products as they "
            "relate to each other based on their cluster IDs. This will only "
            "be called while the UI for the feature is open."
          trigger:
            "When the Tab Compare UI is opened, we will send a request any "
            "time the list of currently viewed products changes."
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
          chrome_policy {
            TabCompareSettings {
              policy_options {mode: MANDATORY}
              TabCompareSettings: 2
            }
          }
        })");

std::optional<ProductSpecifications::DescriptionText> ParseDescriptionText(
    const base::Value::Dict* desc_text_dict) {
  if (!desc_text_dict) {
    return std::nullopt;
  }

  std::optional<ProductSpecifications::DescriptionText> description;
  description.emplace();

  const std::string* description_text = desc_text_dict->FindString(kTextKey);

  const base::Value::List* url_list = desc_text_dict->FindList(kUrlsKey);
  if (url_list) {
    for (const auto& url_object : *url_list) {
      if (!url_object.is_dict()) {
        continue;
      }
      const std::string* url_string = url_object.GetDict().FindString(kUrlKey);
      const std::string* title = url_object.GetDict().FindString(kTitleKey);
      const std::string* favicon_url =
          url_object.GetDict().FindString(kFaviconUrlKey);
      const std::string* thumbnail_url =
          url_object.GetDict().FindString(kThumbnailUrlKey);
      description->urls.push_back(UrlInfo(
          GURL(url_string ? *url_string : ""),
          base::UTF8ToUTF16(title ? *title : ""),
          favicon_url ? std::make_optional(GURL(*favicon_url)) : std::nullopt,
          thumbnail_url ? std::make_optional(GURL(*thumbnail_url))
                        : std::nullopt));
    }
  }

  description->text = description_text ? *description_text : "";

  return description;
}

std::optional<ProductSpecifications::Description> ParseDescription(
    const base::Value::Dict* desc_dict) {
  if (!desc_dict) {
    return std::nullopt;
  }

  std::optional<ProductSpecifications::Description> description;
  description.emplace();

  const std::string* label = desc_dict->FindString(kLabelKey);
  const std::string* alt_text = desc_dict->FindString(kAltTextKey);

  description->label = label ? *label : "";
  description->alt_text = alt_text ? *alt_text : "";

  const base::Value::List* options = desc_dict->FindList(kOptionsKey);

  if (options) {
    for (const auto& option_value : *options) {
      ProductSpecifications::Description::Option option;

      if (!option_value.is_dict()) {
        continue;
      }

      const base::Value::List* desc_list =
          option_value.GetIfDict()->FindList(kDescriptionKey);
      if (desc_list) {
        for (const auto& list_item : *desc_list) {
          std::optional<ProductSpecifications::DescriptionText> desc_text =
              ParseDescriptionText(list_item.GetIfDict());
          if (desc_text.has_value()) {
            option.descriptions.push_back(desc_text.value());
          }
        }
      }

      description->options.push_back(std::move(option));
    }
  }

  return description;
}

std::optional<ProductSpecifications::Value> ParseValue(
    const base::Value::Dict* value_dict) {
  if (!value_dict) {
    return std::nullopt;
  }

  ProductSpecifications::Value value;

  // Process value description
  const base::Value::List* specs_descriptions_list =
      value_dict->FindList(kSpecificationDescriptionsKey);
  if (specs_descriptions_list) {
    for (const auto& spec_description : *specs_descriptions_list) {
      std::optional<ProductSpecifications::Description> description =
          ParseDescription(spec_description.GetIfDict());
      if (description.has_value()) {
        value.descriptions.push_back(description.value());
      }
    }
  }

  const base::Value::List* summary_descriptions_list =
      value_dict->FindList(kSummaryKey);
  if (summary_descriptions_list) {
    for (const auto& summary_item : *summary_descriptions_list) {
      std::optional<ProductSpecifications::DescriptionText> summary =
          ParseDescriptionText(summary_item.GetIfDict());
      if (summary.has_value()) {
        value.summary.push_back(summary.value());
      }
    }
  }

  return value;
}

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
  if (!CanFetchProductSpecificationsData(account_checker_)) {
    std::move(callback).Run(cluster_ids, std::nullopt);
    return;
  }

  std::string specs_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kProductSpecificationsUrlKey);
  if (specs_url.empty()) {
    specs_url = kEndpointUrl;
  }

  auto fetcher =
      CreateEndpointFetcher(GURL(specs_url), kPostHttpMethod,
                            GetJsonStringForProductClusterIds(cluster_ids));

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

    const base::Value::List* summary_list =
        spec.GetDict().FindList(kSummaryKey);
    if (summary_list) {
      for (const auto& summary_item : *summary_list) {
        std::optional<ProductSpecifications::DescriptionText> summary =
            ParseDescriptionText(summary_item.GetIfDict());
        if (summary.has_value()) {
          product.summary.push_back(summary.value());
        }
      }
    }

    const std::string* image_url = spec.GetDict().FindString(kImageURLKey);
    if (image_url) {
      product.image_url = GURL(*image_url);
    }

    const std::string* buying_options_url =
        spec.GetDict().FindString(kBuyingOptionsURLKey);
    if (buying_options_url) {
      product.buying_options_url = GURL(*buying_options_url);
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

      std::optional<ProductSpecifications::Value> value_object =
          ParseValue(spec_value.GetIfDict());

      if (value_object.has_value()) {
        product.product_dimension_values[value_id] = value_object.value();
      }
    }

    product_specs->products.push_back(product);
  }

  return product_specs;
}

}  // namespace commerce
