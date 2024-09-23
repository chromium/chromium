// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/internals/commerce_internals_handler.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/webui/webui_utils.h"
#include "components/payments/core/currency_formatter.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "ui/webui/resources/cr_components/commerce/shopping_service.mojom.h"

namespace {

shopping_service::mojom::BookmarkProductInfoPtr GetBookmarkProductInfo(
    const bookmarks::BookmarkNode* bookmark,
    power_bookmarks::PowerBookmarkMeta* meta,
    const std::string& locale_on_startup) {
  const power_bookmarks::ShoppingSpecifics& specifics =
      meta->shopping_specifics();
  auto bookmark_info = shopping_service::mojom::BookmarkProductInfo::New();
  bookmark_info->bookmark_id = bookmark->id();
  bookmark_info->info = shopping_service::mojom::ProductInfo::New();
  bookmark_info->info->title = specifics.title();
  bookmark_info->info->product_url = bookmark->url();
  bookmark_info->info->domain = base::UTF16ToUTF8(
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          GURL(bookmark->url())));
  bookmark_info->info->image_url = GURL(meta->lead_image().url());
  bookmark_info->info->cluster_id = specifics.product_cluster_id();
  const power_bookmarks::ProductPrice price = specifics.current_price();

  std::unique_ptr<payments::CurrencyFormatter> formatter =
      std::make_unique<payments::CurrencyFormatter>(price.currency_code(),
                                                    locale_on_startup);
  formatter->SetMaxFractionalDigits(2);

  bookmark_info->info->current_price = base::UTF16ToUTF8(formatter->Format(
      base::NumberToString(static_cast<float>(price.amount_micros()) /
                           commerce::kToMicroCurrency)));
  if (specifics.has_previous_price() &&
      specifics.previous_price().amount_micros() >
          specifics.current_price().amount_micros()) {
    const power_bookmarks::ProductPrice previous_price =
        specifics.previous_price();
    bookmark_info->info->previous_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(
            static_cast<float>(previous_price.amount_micros()) /
            commerce::kToMicroCurrency)));
  }
  return bookmark_info;
}

std::vector<commerce::mojom::SubscriptionPtr> GetSubscriptionsMojom(
    bookmarks::BookmarkModel* bookmark_model,
    const std::string& locale_on_startup,
    std::vector<commerce::CommerceSubscription> subscriptions) {
  std::vector<commerce::mojom::SubscriptionPtr> subscription_list;
  for (auto sub : subscriptions) {
    uint64_t cluster_id;
    if (base::StringToUint64(sub.id, &cluster_id)) {
      std::vector<const bookmarks::BookmarkNode*> bookmarks =
          commerce::GetBookmarksWithClusterId(bookmark_model, cluster_id);

      std::vector<shopping_service::mojom::BookmarkProductInfoPtr> info_list;
      for (auto* bookmark : bookmarks) {
        std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
            power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model, bookmark);
        if (!meta || !meta->has_shopping_specifics()) {
          continue;
        }
        info_list.push_back(
            GetBookmarkProductInfo(bookmark, meta.get(), locale_on_startup));
      }
      commerce::mojom::SubscriptionPtr subscription =
          commerce::mojom::Subscription::New();
      subscription->cluster_id = cluster_id;
      subscription->product_infos = std::move(info_list);
      subscription_list.push_back(std::move(subscription));
    }
  }
  return subscription_list;
}

}  // namespace

namespace commerce {

CommerceInternalsHandler::CommerceInternalsHandler(
    mojo::PendingRemote<mojom::CommerceInternalsPage> page,
    mojo::PendingReceiver<mojom::CommerceInternalsHandler> receiver,
    ShoppingService* shopping_service)
    : page_(std::move(page)),
      receiver_(this, std::move(receiver)),
      shopping_service_(shopping_service) {
  if (!shopping_service_) {
    return;
  }
  shopping_service_->WaitForReady(base::BindOnce(
      [](base::WeakPtr<CommerceInternalsHandler> handler,
         ShoppingService* service) {
        if (handler.WasInvalidated()) {
          return;
        }

        // This will happen in tests that don't pass CHECK_IS_TEST.
        if (!service) {
          return;
        }

        handler->page_->OnShoppingListEligibilityChanged(
            service->IsShoppingListEligible());
      },
      weak_ptr_factory_.GetWeakPtr()));
}

CommerceInternalsHandler::~CommerceInternalsHandler() = default;

void CommerceInternalsHandler::GetIsShoppingListEligible(
    GetIsShoppingListEligibleCallback callback) {
  std::move(callback).Run(
      shopping_service_ ? shopping_service_->IsShoppingListEligible() : false);
}

void CommerceInternalsHandler::GetShoppingListEligibleDetails(
    GetShoppingListEligibleDetailsCallback callback) {
  mojom::ShoppingListEligibleDetailPtr detail =
      mojom::ShoppingListEligibleDetail::New();

  if (!shopping_service_) {
    std::move(callback).Run(std::move(detail));
    return;
  }

  detail->is_region_locked_feature_enabled = mojom::EligibleEntry::New(
      IsRegionLockedFeatureEnabled(kShoppingList, kShoppingListRegionLaunched,
                                   shopping_service_->country_on_startup_,
                                   shopping_service_->locale_on_startup_),
      /*expected_value=*/true);
  detail->is_shopping_list_allowed_for_enterprise = mojom::EligibleEntry::New(
      shopping_service_->pref_service_ &&
          IsShoppingListAllowedForEnterprise(shopping_service_->pref_service_),
      /*expected_value=*/true);

  auto* account_checker = shopping_service_->account_checker_.get();
  if (!account_checker) {
    detail->is_account_checker_valid =
        mojom::EligibleEntry::New(false, /*expected_value=*/true);
    std::move(callback).Run(std::move(detail));
    return;
  }
  detail->is_account_checker_valid =
      mojom::EligibleEntry::New(true, /*expected_value=*/true);
  detail->is_signed_in =
      mojom::EligibleEntry::New(account_checker->IsSignedIn(),
                                /*expected_value=*/true);
  detail->is_syncing_bookmarks = mojom::EligibleEntry::New(
      account_checker->IsSyncingBookmarks(), /*expected_value=*/true);
  detail->is_anonymized_url_data_collection_enabled = mojom::EligibleEntry::New(
      account_checker->IsAnonymizedUrlDataCollectionEnabled(),
      /*expected_value=*/true);
  detail->is_subject_to_parental_controls =
      mojom::EligibleEntry::New(account_checker->IsSubjectToParentalControls(),
                                /*expected_value=*/false);

  std::move(callback).Run(std::move(detail));
}

void CommerceInternalsHandler::ResetPriceTrackingEmailPref() {
  if (!shopping_service_) {
    return;
  }
  shopping_service_->pref_service_->ClearPref(kPriceEmailNotificationsEnabled);
}

void CommerceInternalsHandler::GetProductInfoForUrl(
    const GURL& url,
    GetProductInfoForUrlCallback callback) {
  if (!shopping_service_) {
    std::move(callback).Run(shopping_service::mojom::ProductInfo::New());
    return;
  }

  shopping_service_->GetProductInfoForUrl(
      url,
      base::BindOnce(
          [](GetProductInfoForUrlCallback callback,
             base::WeakPtr<ShoppingService> service, const GURL& url,
             const std::optional<const ProductInfo>& info) {
            if (!service || !info) {
              std::move(callback).Run(
                  shopping_service::mojom::ProductInfo::New());
              return;
            }

            std::move(callback).Run(ProductInfoToMojoProduct(
                url, info, service->locale_on_startup_));
          },
          std::move(callback), shopping_service_->AsWeakPtr()));
}

void CommerceInternalsHandler::GetSubscriptionDetails(
    GetSubscriptionDetailsCallback callback) {
  if (!shopping_service_->IsShoppingListEligible()) {
    std::vector<commerce::mojom::SubscriptionPtr> subscription_list;
    std::move(callback).Run(std::move(subscription_list));
    return;
  }

  shopping_service_->GetAllSubscriptions(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](GetSubscriptionDetailsCallback callback,
             base::WeakPtr<bookmarks::BookmarkModel> bookmark_model,
             const std::string& locale_on_startup,
             std::vector<CommerceSubscription> subscriptions) {
            std::move(callback).Run(GetSubscriptionsMojom(
                bookmark_model.get(), locale_on_startup, subscriptions));
          },
          std::move(callback), shopping_service_->bookmark_model_->AsWeakPtr(),
          shopping_service_->locale_on_startup_));
}

void CommerceInternalsHandler::GetProductSpecificationsDetails(
    GetProductSpecificationsDetailsCallback callback) {
  std::vector<commerce::mojom::ProductSpecificationsSetPtr>
      product_specifications_list;

  for (auto& spec : shopping_service_->GetAllProductSpecificationSets()) {
    commerce::mojom::ProductSpecificationsSetPtr product_specifications =
        commerce::mojom::ProductSpecificationsSet::New();
    product_specifications->uuid = spec.uuid().AsLowercaseString();
    product_specifications->creation_time = base::UTF16ToUTF8(
        base::TimeFormatShortDateAndTime(spec.creation_time()));
    product_specifications->update_time =
        base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(spec.update_time()));
    product_specifications->name = spec.name();
    auto& url_infos = product_specifications->url_infos;
    for (const UrlInfo& url_info : spec.url_infos()) {
      auto url_info_ptr = shopping_service::mojom::UrlInfo::New();
      url_info_ptr->url = url_info.url;
      url_info_ptr->title = base::UTF16ToUTF8(url_info.title);
      url_infos.push_back(std::move(url_info_ptr));
    }
    product_specifications_list.push_back(std::move(product_specifications));
  }
  std::move(callback).Run(std::move(product_specifications_list));
}

void CommerceInternalsHandler::ResetProductSpecifications() {
  auto* product_specifications_service =
      shopping_service_->GetProductSpecificationsService();
  if (!product_specifications_service) {
    return;
  }
  shopping_service_->pref_service_->SetInteger(
      commerce::kProductSpecificationsEntryPointShowIntervalInDays, 0);
  shopping_service_->pref_service_->SetTime(
      commerce::kProductSpecificationsEntryPointLastDismissedTime,
      base::Time::Now());
  shopping_service_->pref_service_->SetInteger(
      commerce::kProductSpecificationsAcceptedDisclosureVersion,
      static_cast<int>(shopping_service::mojom::
                           ProductSpecificationsDisclosureVersion::kUnknown));
  product_specifications_service->GetAllProductSpecifications(base::BindOnce(
      &CommerceInternalsHandler::DeleteAllProductSpecificationSets,
      weak_ptr_factory_.GetWeakPtr()));
}

void CommerceInternalsHandler::DeleteAllProductSpecificationSets(
    const std::vector<ProductSpecificationsSet> sets) {
  auto* product_specifications_service =
      shopping_service_->GetProductSpecificationsService();
  for (auto& set : sets) {
    product_specifications_service->DeleteProductSpecificationsSet(
        set.uuid().AsLowercaseString());
  }
}
}  // namespace commerce
