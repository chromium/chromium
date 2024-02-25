// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/discounts_storage.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/proto/discounts_db_content.pb.h"
#include "components/session_proto_db/session_proto_storage.h"

namespace commerce {

const char kDiscountsFetchResultHistogramName[] =
    "Commerce.Discounts.FetchResult";

namespace {
bool MatchPrefixBeforeUTM(const std::string& url_with_utm,
                          const std::string& target_url) {
  size_t index = url_with_utm.find(commerce::kUTMPrefix);
  if (index == std::string::npos) {
    return false;
  }
  // Offset the '?' which is the starting character of URL parameters. We need
  // this offset in case the target URL doesn't have any URL parameter.
  if (index > 0 && url_with_utm[index - 1] == '?') {
    index--;
  }
  const std::string url_prefix = url_with_utm.substr(0, index);
  return base::StartsWith(target_url, url_prefix);
}
}  // namespace

DiscountsStorage::DiscountsStorage(
    SessionProtoStorage<DiscountsContent>* discounts_proto_db,
    history::HistoryService* history_service)
    : proto_db_(discounts_proto_db) {
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
}
DiscountsStorage::~DiscountsStorage() = default;

void DiscountsStorage::HandleServerDiscounts(
    const std::vector<std::string>& urls_to_check,
    DiscountsMap server_results,
    DiscountInfoCallback callback) {
  // Update local database with server-fetched results.
  for (const auto& result : server_results) {
    CHECK(result.second.size() > 0);
    SaveDiscounts(result.first, result.second);
  }

  if (urls_to_check.size() == 0) {
    std::move(callback).Run(std::move(server_results));
  } else {
    proto_db_->LoadAllEntries(base::BindOnce(
        &DiscountsStorage::OnLoadAllDiscounts, weak_ptr_factory_.GetWeakPtr(),
        urls_to_check, std::move(server_results), std::move(callback)));
  }
}

void DiscountsStorage::SaveDiscounts(const GURL& url,
                                     const std::vector<DiscountInfo>& infos) {
  DiscountsContent proto;
  proto.set_key(url.spec());
  for (const DiscountInfo& info : infos) {
    discounts_db::DiscountContent* discount_proto = proto.add_discounts();
    discounts_db::DiscountContent_ClusterType cluster_type =
        discounts_db::DiscountContent_ClusterType_CLUSTER_TYPE_UNSPECIFIED;
    if (info.cluster_type == DiscountClusterType::kOfferLevel) {
      cluster_type = discounts_db::DiscountContent_ClusterType_OFFER_LEVEL;
    }
    discount_proto->set_cluster_type(cluster_type);
    discount_proto->set_id(info.id);
    discounts_db::DiscountContent_Type type =
        discounts_db::DiscountContent_Type_TYPE_UNSPECIFIED;
    if (info.type == DiscountType::kFreeListingWithCode) {
      type = discounts_db::DiscountContent_Type_FREE_LISTING_WITH_CODE;
    }
    discount_proto->set_type(type);
    discount_proto->set_language_code(info.language_code);
    discount_proto->set_description_detail(info.description_detail);
    if (info.terms_and_conditions.has_value()) {
      discount_proto->set_terms_and_conditions(
          info.terms_and_conditions.value());
    }
    discount_proto->set_value_in_text(info.value_in_text);
    discount_proto->set_expiry_time_sec(info.expiry_time_sec);
    discount_proto->set_is_merchant_wide(info.is_merchant_wide);
    if (info.discount_code.has_value()) {
      discount_proto->set_discount_code(info.discount_code.value());
    }
    discount_proto->set_offer_id(info.offer_id);
  }

  proto_db_->InsertContent(url.spec(), proto,
                           base::BindOnce([](bool succeeded) {}));
}

void DiscountsStorage::DeleteDiscountsForUrl(const std::string& url) {
  proto_db_->DeleteOneEntry(url, base::BindOnce([](bool succeeded) {}));
}

void DiscountsStorage::OnLoadAllDiscounts(
    const std::vector<std::string>& urls_to_check,
    DiscountsMap server_results,
    DiscountInfoCallback callback,
    bool succeeded,
    DiscountsKeyAndValues data) {
  if (!succeeded) {
    std::move(callback).Run(std::move(server_results));
    return;
  }

  int urls_found_in_db_number = 0;
  for (const std::string& url_to_check : urls_to_check) {
    for (SessionProtoStorage<DiscountsContent>::KeyAndValue& kv : data) {
      if (url_to_check == kv.first) {
        urls_found_in_db_number++;
        std::vector<DiscountInfo> infos =
            GetUnexpiredDiscountsFromProto(kv.second);
        if (infos.size() == 0) {
          DeleteDiscountsForUrl(kv.first);
          base::UmaHistogramEnumeration(kDiscountsFetchResultHistogramName,
                                        DiscountsFetchResult::kInvalidInfoInDb);
        } else {
          server_results[GURL(kv.first)] = infos;
          base::UmaHistogramEnumeration(kDiscountsFetchResultHistogramName,
                                        DiscountsFetchResult::kValidInfoInDb);

          // Update local database if expired discounts found.
          if ((int)(infos.size()) != kv.second.discounts().size()) {
            SaveDiscounts(GURL(kv.first), infos);
          }
        }
      } else if (commerce::UrlContainsDiscountUtmTag(GURL(url_to_check)) &&
                 MatchPrefixBeforeUTM(url_to_check, kv.first)) {
        // If the `url_to_check` contains discount UTM but has no corresponding
        // discount from server, try to see if we can find a URL that has
        // discount in storage and points to the same product by matching URL
        // prefix.
        std::vector<DiscountInfo> infos =
            GetUnexpiredDiscountsFromProto(kv.second);
        if (infos.size() != 0) {
          server_results[GURL(url_to_check)] = infos;
        }
      }
    }
  }

  int urls_not_found_number = urls_to_check.size() - urls_found_in_db_number;
  for (int i = 0; i < urls_not_found_number; i++) {
    base::UmaHistogramEnumeration(kDiscountsFetchResultHistogramName,
                                  DiscountsFetchResult::kInfoNotFound);
  }

  std::move(callback).Run(std::move(server_results));
}

std::vector<DiscountInfo> DiscountsStorage::GetUnexpiredDiscountsFromProto(
    const DiscountsContent& proto) {
  std::vector<DiscountInfo> infos;
  for (const discounts_db::DiscountContent& content : proto.discounts()) {
    // First check whether the discount is expired.
    if ((base::Time::Now() - base::Time::UnixEpoch()).InSeconds() >
        content.expiry_time_sec()) {
      continue;
    }

    DiscountInfo info;
    info.cluster_type = DiscountClusterType(content.cluster_type());
    info.id = content.id();
    info.type = DiscountType(content.type());
    info.language_code = content.language_code();
    info.description_detail = content.description_detail();
    if (content.has_terms_and_conditions()) {
      info.terms_and_conditions = content.terms_and_conditions();
    }
    info.value_in_text = content.value_in_text();
    info.expiry_time_sec = content.expiry_time_sec();
    info.is_merchant_wide = content.is_merchant_wide();
    if (content.has_discount_code()) {
      info.discount_code = content.discount_code();
    }
    info.offer_id = content.offer_id();
    infos.push_back(info);
  }

  return infos;
}

void DiscountsStorage::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory()) {
    proto_db_->DeleteAllContent(base::BindOnce([](bool succeeded) {}));
    return;
  }

  for (const history::URLRow& row : deletion_info.deleted_rows()) {
    DeleteDiscountsForUrl(row.url().spec());
  }
}

}  // namespace commerce
