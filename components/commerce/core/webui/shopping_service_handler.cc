// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/shopping_service_handler.h"

#include <memory>
#include <vector>

#include "base/check_is_test.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/metrics/metrics_utils.h"
#include "components/commerce/core/mojom/shared.mojom.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/webui/webui_utils.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/features/product_specifications.pb.h"
#include "components/payments/core/currency_formatter.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "url/gurl.h"

namespace commerce {
namespace {

shopping_service::mojom::UrlInfoPtr UrlInfoToMojo(const UrlInfo& url_info) {
  auto url_info_ptr = shopping_service::mojom::UrlInfo::New();
  url_info_ptr->url = url_info.url;
  url_info_ptr->title = base::UTF16ToUTF8(url_info.title);
  url_info_ptr->previewText = url_info.previewText.value_or("");
  url_info_ptr->favicon_url = url_info.favicon_url.value_or(GURL());
  return url_info_ptr;
}

std::vector<shopping_service::mojom::UrlInfoPtr> UrlInfoListToMojo(
    const std::vector<UrlInfo>& url_infos) {
  std::vector<shopping_service::mojom::UrlInfoPtr> url_info_ptr_list;

  for (const UrlInfo& url_info : url_infos) {
    url_info_ptr_list.push_back(UrlInfoToMojo(url_info));
  }
  return url_info_ptr_list;
}

shopping_service::mojom::PriceInsightsInfoPtr PriceInsightsInfoToMojoObject(
    const std::optional<PriceInsightsInfo>& info,
    const std::string& locale) {
  auto insights_info = shopping_service::mojom::PriceInsightsInfo::New();

  if (!info.has_value()) {
    return insights_info;
  }

  if (info->product_cluster_id.has_value()) {
    insights_info->cluster_id = info->product_cluster_id.value();
  } else {
    CHECK_IS_TEST();
  }

  std::unique_ptr<payments::CurrencyFormatter> formatter =
      std::make_unique<payments::CurrencyFormatter>(info->currency_code,
                                                    locale);
  formatter->SetMaxFractionalDigits(2);

  if (info->typical_low_price_micros.has_value() &&
      info->typical_high_price_micros.has_value()) {
    insights_info->typical_low_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(
            static_cast<float>(info->typical_low_price_micros.value()) /
            kToMicroCurrency)));

    insights_info->typical_high_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(
            static_cast<float>(info->typical_high_price_micros.value()) /
            kToMicroCurrency)));
  }

  if (info->catalog_attributes.has_value()) {
    insights_info->catalog_attributes = info->catalog_attributes.value();
  }

  if (info->jackpot_url.has_value()) {
    insights_info->jackpot = info->jackpot_url.value();
  }

  shopping_service::mojom::PriceInsightsInfo::PriceBucket bucket;
  switch (info->price_bucket) {
    case PriceBucket::kUnknown:
      bucket =
          shopping_service::mojom::PriceInsightsInfo::PriceBucket::kUnknown;
      break;
    case PriceBucket::kLowPrice:
      bucket = shopping_service::mojom::PriceInsightsInfo::PriceBucket::kLow;
      break;
    case PriceBucket::kTypicalPrice:
      bucket =
          shopping_service::mojom::PriceInsightsInfo::PriceBucket::kTypical;
      break;
    case PriceBucket::kHighPrice:
      bucket = shopping_service::mojom::PriceInsightsInfo::PriceBucket::kHigh;
      break;
  }
  insights_info->bucket = bucket;

  insights_info->has_multiple_catalogs = info->has_multiple_catalogs;

  for (auto history_price : info->catalog_history_prices) {
    auto point = shopping_service::mojom::PricePoint::New();
    point->date = std::get<0>(history_price);

    auto price =
        static_cast<float>(std::get<1>(history_price)) / kToMicroCurrency;
    point->price = price;
    point->formatted_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(price)));
    insights_info->history.push_back(std::move(point));
  }

  insights_info->locale = locale;
  insights_info->currency_code = info->currency_code;

  return insights_info;
}

shopping_service::mojom::ProductSpecificationsDescriptionTextPtr
DescriptionTextToMojo(const ProductSpecifications::DescriptionText& desc_text) {
  auto desc_text_ptr =
      shopping_service::mojom::ProductSpecificationsDescriptionText::New();
  desc_text_ptr->text = desc_text.text;
  for (const auto& url_info : desc_text.urls) {
    if (!url_info.url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    desc_text_ptr->urls.push_back(UrlInfoToMojo(url_info));
  }
  return desc_text_ptr;
}

shopping_service::mojom::ProductSpecificationsPtr ProductSpecsToMojo(
    const ProductSpecifications& specs) {
  auto specs_ptr = shopping_service::mojom::ProductSpecifications::New();

  for (const auto& [id, name] : specs.product_dimension_map) {
    specs_ptr->product_dimension_map[id] = name;
  }

  for (const auto& product : specs.products) {
    auto product_ptr =
        shopping_service::mojom::ProductSpecificationsProduct::New();
    product_ptr->product_cluster_id = product.product_cluster_id;
    product_ptr->title = product.title;
    product_ptr->image_url = product.image_url;
    product_ptr->buying_options_url = product.buying_options_url;

    // Top-level product summaries.
    for (const auto& summary : product.summary) {
      product_ptr->summary.push_back(DescriptionTextToMojo(summary));
    }

    for (const auto& [dimen_id, value] : product.product_dimension_values) {
      auto value_ptr =
          shopping_service::mojom::ProductSpecificationsValue::New();

      // Summaries for the dimension as a whole.
      for (const auto& summary : value.summary) {
        value_ptr->summary.push_back(DescriptionTextToMojo(summary));
      }

      for (const auto& description : value.descriptions) {
        auto desc_ptr =
            shopping_service::mojom::ProductSpecificationsDescription::New();
        desc_ptr->label = description.label;
        desc_ptr->alt_text = description.alt_text;

        for (const auto& option : description.options) {
          auto option_ptr =
              shopping_service::mojom::ProductSpecificationsOption::New();

          for (const auto& description_text : option.descriptions) {
            option_ptr->descriptions.push_back(
                DescriptionTextToMojo(description_text));
          }

          desc_ptr->options.push_back(std::move(option_ptr));
        }

        value_ptr->specification_descriptions.push_back(std::move(desc_ptr));
      }

      product_ptr->product_dimension_values.insert_or_assign(
          dimen_id, std::move(value_ptr));
    }

    specs_ptr->products.push_back(std::move(product_ptr));
  }

  return specs_ptr;
}

std::unique_ptr<optimization_guide::ModelQualityLogEntry>
PrepareQualityLogEntry(optimization_guide::ModelQualityLogsUploaderService*
                           model_quality_logs_uploader_service) {
  if (!model_quality_logs_uploader_service) {
    return nullptr;
  }
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry =
      std::make_unique<optimization_guide::ModelQualityLogEntry>(
          model_quality_logs_uploader_service->GetWeakPtr());
  optimization_guide::proto::LogAiDataRequest* request =
      log_entry->log_ai_data_request();
  if (!request) {
    return nullptr;
  }
  // Generate an execution_id here for logging purpose.
  std::string log_id = commerce::kProductSpecificationsLoggingPrefix +
                       base::Uuid::GenerateRandomV4().AsLowercaseString();
  request->mutable_model_execution_info()->set_execution_id(std::move(log_id));
  return log_entry;
}

void ConvertDescriptionTextToProto(
    ProductSpecifications::DescriptionText description_text,
    optimization_guide::proto::DescriptionText* description_proto) {
  description_proto->set_text(description_text.text);
  for (auto url_info : description_text.urls) {
    optimization_guide::proto::DescriptionText::ReferenceUrl* reference_url =
        description_proto->add_urls();
    reference_url->set_url(url_info.url.spec());
    reference_url->set_title(base::UTF16ToUTF8(url_info.title));
    if (url_info.favicon_url.has_value()) {
      reference_url->set_favicon_url(url_info.favicon_url->spec());
    }
    if (url_info.thumbnail_url.has_value()) {
      reference_url->set_thumbail_image_url(url_info.thumbnail_url->spec());
    }
  }
}

void ConvertProductSpecificationsToProto(
    ProductSpecifications specs,
    optimization_guide::proto::ProductSpecificationData* product_spec_data) {
  for (auto pair : specs.product_dimension_map) {
    optimization_guide::proto::ProductSpecificationSection* section =
        product_spec_data->add_product_specification_sections();
    section->set_key(base::NumberToString(pair.first));
    section->set_title(pair.second);
  }

  for (auto product : specs.products) {
    optimization_guide::proto::ProductSpecification* product_spec =
        product_spec_data->add_product_specifications();
    product_spec->mutable_identifiers()->set_gpc_id(product.product_cluster_id);
    product_spec->mutable_identifiers()->set_mid(product.mid);
    product_spec->set_title(product.title);
    product_spec->set_image_url(product.image_url.spec());

    for (auto pair : product.product_dimension_values) {
      optimization_guide::proto::ProductSpecificationValue*
          product_specification_value =
              product_spec->add_product_specification_values();
      product_specification_value->set_key(base::NumberToString(pair.first));

      for (auto description : pair.second.descriptions) {
        optimization_guide::proto::ProductSpecificationDescription*
            product_specification_description =
                product_specification_value->add_specification_descriptions();
        product_specification_description->set_label(description.label);
        product_specification_description->set_alternative_text(
            description.alt_text);
        for (auto option : description.options) {
          optimization_guide::proto::ProductSpecificationDescription::Option*
              option_proto = product_specification_description->add_options();
          for (auto option_description : option.descriptions) {
            optimization_guide::proto::DescriptionText* description_text =
                option_proto->add_description();
            ConvertDescriptionTextToProto(option_description, description_text);
          }
        }
      }

      for (auto summary : pair.second.summary) {
        optimization_guide::proto::DescriptionText* summary_description =
            product_specification_value->add_summary_description();
        ConvertDescriptionTextToProto(summary, summary_description);
      }
    }

    for (auto description_text : product.summary) {
      optimization_guide::proto::DescriptionText* summary_description =
          product_spec->add_summary_description();
      ConvertDescriptionTextToProto(description_text, summary_description);
    }
  }
}

// TODO(b/347064310): Move this method to some lower-level layers instead of
// here which is only usable in WebUI.
void RecordQualityEntry(
    optimization_guide::proto::ProductSpecificationsQuality* quality_proto,
    std::vector<GURL> input_urls,
    ProductSpecifications specs) {
  // Record input.
  for (auto url : input_urls) {
    optimization_guide::proto::ProductIdentifier* identifider =
        quality_proto->add_product_identifiers();
    identifider->set_product_url(url.spec());
  }
  // Record product spec table.
  optimization_guide::proto::ProductSpecificationData* product_spec_data =
      quality_proto->mutable_product_specification_data();
  ConvertProductSpecificationsToProto(specs, product_spec_data);
}
}  // namespace

using shared::mojom::BookmarkProductInfo;
using shared::mojom::BookmarkProductInfoPtr;

ShoppingServiceHandler::ShoppingServiceHandler(
    mojo::PendingReceiver<shopping_service::mojom::ShoppingServiceHandler>
        receiver,
    bookmarks::BookmarkModel* bookmark_model,
    ShoppingService* shopping_service,
    PrefService* prefs,
    feature_engagement::Tracker* tracker,
    std::unique_ptr<Delegate> delegate,
    optimization_guide::ModelQualityLogsUploaderService*
        model_quality_logs_uploader_service)
    : receiver_(this, std::move(receiver)),
      bookmark_model_(bookmark_model),
      shopping_service_(shopping_service),
      pref_service_(prefs),
      tracker_(tracker),
      delegate_(std::move(delegate)),
      model_quality_logs_uploader_service_(
          model_quality_logs_uploader_service) {
  // It is safe to schedule updates. If the feature is disabled, no new
  // information will be fetched or provided to the frontend.
  shopping_service_->ScheduleSavedProductUpdate();

  if (shopping_service_->GetAccountChecker()) {
    locale_ = shopping_service_->GetAccountChecker()->GetLocale();
  }
}

ShoppingServiceHandler::~ShoppingServiceHandler() = default;

void ShoppingServiceHandler::GetProductInfoForCurrentUrl(
    GetProductInfoForCurrentUrlCallback callback) {
  if (!commerce::IsPriceInsightsEligible(
          shopping_service_->GetAccountChecker()) ||
      !delegate_ || !delegate_->GetCurrentTabUrl().has_value()) {
    std::move(callback).Run(shared::mojom::ProductInfo::New());
    return;
  }

  shopping_service_->GetProductInfoForUrl(
      delegate_->GetCurrentTabUrl().value(),
      base::BindOnce(
          [](base::WeakPtr<ShoppingServiceHandler> handler,
             GetProductInfoForCurrentUrlCallback callback, const GURL& url,
             const std::optional<const ProductInfo>& info) {
            if (!handler) {
              std::move(callback).Run(shared::mojom::ProductInfo::New());
              return;
            }

            std::move(callback).Run(
                ProductInfoToMojoProduct(url, info, handler->locale_));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingServiceHandler::GetProductInfoForUrl(
    const GURL& url,
    GetProductInfoForUrlCallback callback) {
  shopping_service_->GetProductInfoForUrl(
      url, base::BindOnce(
               [](base::WeakPtr<ShoppingServiceHandler> handler,
                  GetProductInfoForUrlCallback callback, const GURL& url,
                  const std::optional<const ProductInfo>& info) {
                 if (!handler) {
                   std::move(callback).Run(url,
                                           shared::mojom::ProductInfo::New());
                   return;
                 }

                 std::move(callback).Run(url, ProductInfoToMojoProduct(
                                                  url, info, handler->locale_));
               },
               weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingServiceHandler::GetProductInfoForUrls(
    const std::vector<GURL>& urls,
    GetProductInfoForUrlsCallback callback) {
  shopping_service_->GetProductInfoForUrls(
      urls,
      base::BindOnce(
          [](base::WeakPtr<ShoppingServiceHandler> handler,
             std::vector<GURL> urls, GetProductInfoForUrlsCallback callback,
             const std::map<GURL, std::optional<ProductInfo>> info_map) {
            std::vector<shared::mojom::ProductInfoPtr> info_list;
            // Provide the URLs/info in the same order they were requested.
            for (GURL& url : urls) {
              info_list.push_back(ProductInfoToMojoProduct(
                  url, info_map.at(url), handler->locale_));
            }
            std::move(callback).Run(std::move(info_list));
          },
          weak_ptr_factory_.GetWeakPtr(), urls, std::move(callback)));
}

void ShoppingServiceHandler::IsShoppingListEligible(
    IsShoppingListEligibleCallback callback) {
  std::move(callback).Run(shopping_service_->IsShoppingListEligible());
}

void ShoppingServiceHandler::GetPriceTrackingStatusForCurrentUrl(
    GetPriceTrackingStatusForCurrentUrlCallback callback) {
  // The URL may or may not have a bookmark associated with it. Prioritize
  // accessing the product info for the URL before looking at an existing
  // bookmark.
  shopping_service_->GetProductInfoForUrl(
      delegate_->GetCurrentTabUrl().value(),
      base::BindOnce(
          [](base::WeakPtr<ShoppingServiceHandler> handler,
             GetPriceTrackingStatusForCurrentUrlCallback callback,
             const GURL& url, const std::optional<const ProductInfo>& info) {
            if (!info.has_value() || !info->product_cluster_id.has_value() ||
                !handler || !CanTrackPrice(info)) {
              std::move(callback).Run(false);
              return;
            }
            CommerceSubscription sub(
                SubscriptionType::kPriceTrack,
                IdentifierType::kProductClusterId,
                base::NumberToString(info->product_cluster_id.value()),
                ManagementType::kUserManaged);

            handler->shopping_service_->IsSubscribed(
                sub,
                base::BindOnce(
                    [](GetPriceTrackingStatusForCurrentUrlCallback callback,
                       bool subscribed) {
                      std::move(callback).Run(subscribed);
                    },
                    std::move(callback)));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingServiceHandler::GetPriceInsightsInfoForCurrentUrl(
    GetPriceInsightsInfoForCurrentUrlCallback callback) {
  if (!commerce::IsPriceInsightsEligible(
          shopping_service_->GetAccountChecker()) ||
      !delegate_ || !delegate_->GetCurrentTabUrl().has_value()) {
    std::move(callback).Run(shopping_service::mojom::PriceInsightsInfo::New());
    return;
  }

  shopping_service_->GetPriceInsightsInfoForUrl(
      delegate_->GetCurrentTabUrl().value(),
      base::BindOnce(
          &ShoppingServiceHandler::OnFetchPriceInsightsInfoForCurrentUrl,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingServiceHandler::GetPriceInsightsInfoForUrl(
    const GURL& url,
    GetPriceInsightsInfoForUrlCallback callback) {
  if (!commerce::IsPriceInsightsEligible(
          shopping_service_->GetAccountChecker())) {
    std::move(callback).Run(url,
                            shopping_service::mojom::PriceInsightsInfo::New());
    return;
  }

  shopping_service_->GetPriceInsightsInfoForUrl(
      url, base::BindOnce(
               [](base::WeakPtr<ShoppingServiceHandler> handler,
                  GetPriceInsightsInfoForUrlCallback callback, const GURL& url,
                  const std::optional<PriceInsightsInfo>& info) {
                 if (!handler) {
                   return;
                 }

                 std::move(callback).Run(url, PriceInsightsInfoToMojoObject(
                                                  info, handler->locale_));
               },
               weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingServiceHandler::GetProductSpecificationsForUrls(
    const std::vector<::GURL>& urls,
    GetProductSpecificationsForUrlsCallback callback) {
  if (!shopping_service_ || urls.empty()) {
    std::move(callback).Run(
        shopping_service::mojom::ProductSpecifications::New());
    return;
  }
  // The data is sent when `current_log_quality_entry_` destructs, which will
  // happen when (1) the page is closed and ShoppingServiceHandler destructs or
  // (2) there is another `GetProductSpecificationsForUrls` call which creates a
  // new current entry and the old one will destruct.
  if (kProductSpecificationsEnableQualityLogging.Get() &&
      IsProductSpecificationsQualityLoggingAllowed(
          shopping_service_->GetAccountChecker()->GetPrefs())) {
    current_log_quality_entry_ =
        PrepareQualityLogEntry(model_quality_logs_uploader_service_);
  }
  shopping_service_->GetProductSpecificationsForUrls(
      urls, base::BindOnce(
                &ShoppingServiceHandler::OnGetProductSpecificationsForUrls,
                weak_ptr_factory_.GetWeakPtr(), urls, std::move(callback)));
}

void ShoppingServiceHandler::GetUrlInfosForProductTabs(
    GetUrlInfosForProductTabsCallback callback) {
  if (!shopping_service_) {
    std::move(callback).Run({});
    return;
  }

  shopping_service_->GetUrlInfosForWebWrappersWithProducts(base::BindOnce(
      [](GetUrlInfosForProductTabsCallback callback,
         const std::vector<UrlInfo> infos) {
        std::move(callback).Run(UrlInfoListToMojo(infos));
      },
      std::move(callback)));
}

void ShoppingServiceHandler::GetUrlInfosForRecentlyViewedTabs(
    GetUrlInfosForRecentlyViewedTabsCallback callback) {
  if (!shopping_service_) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(UrlInfoListToMojo(
      shopping_service_->GetUrlInfosForRecentlyViewedWebWrappers()));
}

void ShoppingServiceHandler::OnFetchPriceInsightsInfoForCurrentUrl(
    GetPriceInsightsInfoForCurrentUrlCallback callback,
    const GURL& url,
    const std::optional<PriceInsightsInfo>& info) {
  std::move(callback).Run(PriceInsightsInfoToMojoObject(info, locale_));
}

void ShoppingServiceHandler::OnGetPriceTrackingStatusForCurrentUrl(
    GetPriceTrackingStatusForCurrentUrlCallback callback,
    bool tracked) {
  std::move(callback).Run(tracked);
}

void ShoppingServiceHandler::OpenUrlInNewTab(const GURL& url) {
  if (delegate_) {
    delegate_->OpenUrlInNewTab(url);
  }
}

void ShoppingServiceHandler::SwitchToOrOpenTab(const GURL& url) {
  if (delegate_) {
    delegate_->SwitchToOrOpenTab(url);
  }
}

void ShoppingServiceHandler::GetAllProductSpecificationsSets(
    GetAllProductSpecificationsSetsCallback callback) {
  if (!shopping_service_ ||
      !shopping_service_->GetProductSpecificationsService()) {
    std::move(callback).Run({});
    return;
  }

  const auto& all_sets = shopping_service_->GetProductSpecificationsService()
                             ->GetAllProductSpecifications();
  std::vector<shared::mojom::ProductSpecificationsSetPtr> all_sets_mojo;
  for (const auto& set : all_sets) {
    all_sets_mojo.push_back(ProductSpecsSetToMojo(set));
  }

  std::move(callback).Run(std::move(all_sets_mojo));
}

void ShoppingServiceHandler::GetProductSpecificationsSetByUuid(
    const base::Uuid& uuid,
    GetProductSpecificationsSetByUuidCallback callback) {
  if (!shopping_service_ ||
      !shopping_service_->GetProductSpecificationsService()) {
    std::move(callback).Run(nullptr);
    return;
  }
  const auto& set =
      shopping_service_->GetProductSpecificationsService()->GetSetByUuid(uuid);
  if (set.has_value()) {
    std::move(callback).Run(ProductSpecsSetToMojo(set.value()));
  } else {
    std::move(callback).Run(nullptr);
  }
}

void ShoppingServiceHandler::AddProductSpecificationsSet(
    const std::string& name,
    const std::vector<GURL>& urls,
    AddProductSpecificationsSetCallback callback) {
  if (!shopping_service_ ||
      !shopping_service_->GetProductSpecificationsService()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::vector<commerce::UrlInfo> url_infos;
  for (const auto& url : urls) {
    url_infos.emplace_back(url, std::u16string());
  }

  std::optional<ProductSpecificationsSet> new_set =
      shopping_service_->GetProductSpecificationsService()
          ->AddProductSpecificationsSet(name, url_infos);

  std::move(callback).Run(
      new_set.has_value() ? ProductSpecsSetToMojo(new_set.value()) : nullptr);
}

void ShoppingServiceHandler::DeleteProductSpecificationsSet(
    const base::Uuid& uuid) {
  if (!shopping_service_ ||
      !shopping_service_->GetProductSpecificationsService()) {
    return;
  }

  shopping_service_->GetProductSpecificationsService()
      ->DeleteProductSpecificationsSet(uuid.AsLowercaseString());
}

void ShoppingServiceHandler::SetNameForProductSpecificationsSet(
    const base::Uuid& uuid,
    const std::string& name,
    SetNameForProductSpecificationsSetCallback callback) {
  if (!shopping_service_ ||
      !shopping_service_->GetProductSpecificationsService()) {
    std::move(callback).Run(nullptr);
    return;
  }
  const auto& set =
      shopping_service_->GetProductSpecificationsService()->SetName(uuid, name);
  std::move(callback).Run(ProductSpecsSetToMojo(set.value()));
}

void ShoppingServiceHandler::SetUrlsForProductSpecificationsSet(
    const base::Uuid& uuid,
    const std::vector<GURL>& urls,
    SetUrlsForProductSpecificationsSetCallback callback) {
  if (!shopping_service_ ||
      !shopping_service_->GetProductSpecificationsService()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // If an url is valid, but longer than mojo can handle, mojo will replace the
  // url with `GURL().` To avoid passing ShoppingService empty urls, we filter
  // them out before passing the list to `SetUrls.`
  std::vector<UrlInfo> valid_url_infos;
  for (const auto& url : urls) {
    if (url.is_valid()) {
      valid_url_infos.emplace_back(url, std::u16string());
    }
  }

  const auto& set =
      shopping_service_->GetProductSpecificationsService()->SetUrls(
          uuid, valid_url_infos);
  if (set.has_value()) {
    std::move(callback).Run(ProductSpecsSetToMojo(set.value()));
  } else {
    std::move(callback).Run(nullptr);
  }
}

void ShoppingServiceHandler::SetProductSpecificationsUserFeedback(
    shopping_service::mojom::UserFeedback feedback) {
  optimization_guide::proto::UserFeedback user_feedback;
  switch (feedback) {
    case shopping_service::mojom::UserFeedback::kThumbsUp:
      user_feedback =
          optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_UP;
      break;
    case shopping_service::mojom::UserFeedback::kThumbsDown:
      user_feedback =
          optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_DOWN;
      break;
    case shopping_service::mojom::UserFeedback::kUnspecified:
      user_feedback =
          optimization_guide::proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;
      break;
  }
  if (user_feedback ==
      optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_DOWN) {
    // If quality log is enabled, `log_id` of the feedback will be the same as
    // the `execution_id` of the log entry so that they can be matched later;
    // otherwise it will be a random ID.
    std::string log_id =
        current_log_quality_entry_
            ? current_log_quality_entry_->log_ai_data_request()
                  ->model_execution_info()
                  .execution_id()
            : commerce::kProductSpecificationsLoggingPrefix +
                  base::Uuid::GenerateRandomV4().AsLowercaseString();
    delegate_->ShowFeedbackForProductSpecifications(std::move(log_id));
  }
  if (!current_log_quality_entry_) {
    return;
  }
  optimization_guide::proto::LogAiDataRequest* request =
      current_log_quality_entry_->log_ai_data_request();
  if (!request) {
    return;
  }
  optimization_guide::proto::ProductSpecificationsQuality* quality_proto =
          request->mutable_product_specifications()
          ->mutable_quality();
  quality_proto->set_user_feedback(user_feedback);
}

void ShoppingServiceHandler::GetProductSpecificationsFeatureState(
    GetProductSpecificationsFeatureStateCallback callback) {
  if (!shopping_service_) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto state_ptr =
      shopping_service::mojom::ProductSpecificationsFeatureState::New();
  state_ptr->is_syncing_tab_compare = commerce::IsSyncingProductSpecifications(
      shopping_service_->GetAccountChecker());
  state_ptr->can_load_full_page_ui =
      commerce::CanLoadProductSpecificationsFullPageUi(
          shopping_service_->GetAccountChecker());
  state_ptr->can_manage_sets = commerce::CanManageProductSpecificationsSets(
      shopping_service_->GetAccountChecker(),
      shopping_service_->GetProductSpecificationsService());
  state_ptr->can_fetch_data = commerce::CanFetchProductSpecificationsData(
      shopping_service_->GetAccountChecker());
  state_ptr->is_allowed_for_enterprise =
      commerce::IsProductSpecificationsAllowedForEnterprise(pref_service_);
  state_ptr->is_quality_logging_allowed =
      commerce::IsProductSpecificationsQualityLoggingAllowed(pref_service_);
  state_ptr->is_signed_in =
      shopping_service_->GetAccountChecker() &&
      shopping_service_->GetAccountChecker()->IsSignedIn();

  std::move(callback).Run(std::move(state_ptr));
  return;
}

void ShoppingServiceHandler::OnGetProductSpecificationsForUrls(
    std::vector<GURL> input_urls,
    GetProductSpecificationsForUrlsCallback callback,
    std::vector<uint64_t> ids,
    std::optional<ProductSpecifications> specs) {
  if (!specs.has_value()) {
    std::move(callback).Run(
        shopping_service::mojom::ProductSpecifications::New());
    return;
  }
  if (current_log_quality_entry_) {
    // Record response in the current log quality entry.
    optimization_guide::proto::LogAiDataRequest* request =
        current_log_quality_entry_->log_ai_data_request();
    if (!request) {
      return;
    }
    RecordQualityEntry(
            request->mutable_product_specifications()
            ->mutable_quality(),
        std::move(input_urls), std::move(specs.value()));
  }

  std::move(callback).Run(ProductSpecsToMojo(specs.value()));
}

}  // namespace commerce
