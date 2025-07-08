// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/discount_infos_storage.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/proto/discounts_db_content.pb.h"
#include "components/session_proto_db/session_proto_storage.h"

namespace commerce {

DiscountInfosStorage::DiscountInfosStorage(
    SessionProtoStorage<DiscountInfosContent>* discount_infos_proto_db,
    history::HistoryService* history_service)
    : proto_db_(discount_infos_proto_db) {
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
}
DiscountInfosStorage::~DiscountInfosStorage() = default;

void DiscountInfosStorage::LoadDiscountsWithPrefix(const GURL& url,
                                         DiscountInfoCallback callback) {
  proto_db_->LoadContentWithPrefix(
      url.spec(),
      base::BindOnce(&DiscountInfosStorage::OnLoadDiscounts,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void DiscountInfosStorage::SaveDiscounts(
    const GURL& url,
    const std::vector<DiscountInfo>& infos) {
  DiscountInfosContent proto;
  proto.set_key(url.spec());
  for (const DiscountInfo& info : infos) {
    if (info.expiry_time_sec.has_value() && info.discount_code.has_value()) {
      discount_infos_db::DiscountInfoContent* discount_proto =
          proto.add_discounts();
      discount_proto->set_id(info.id);
      discount_infos_db::DiscountInfoContent_Type type =
          discount_infos_db::DiscountInfoContent_Type_TYPE_UNSPECIFIED;
      if (info.type == DiscountType::kFreeListingWithCode) {
        type =
            discount_infos_db::DiscountInfoContent_Type_FREE_LISTING_WITH_CODE;
      } else if (info.type == DiscountType::kCrawledPromotion) {
        type = discount_infos_db::DiscountInfoContent_Type_CRAWLED_PROMOTION;
      }
      discount_proto->set_type(type);
      discount_proto->set_language_code(info.language_code);
      discount_proto->set_description_detail(info.description_detail);
      if (info.terms_and_conditions.has_value()) {
        discount_proto->set_terms_and_conditions(
            info.terms_and_conditions.value());
      }
      discount_proto->set_expiry_time_sec(info.expiry_time_sec.value());
      discount_proto->set_discount_code(info.discount_code.value());
      discount_proto->set_offer_id(info.offer_id);
    }
  }
  if (proto.discounts().size() > 0) {
    proto_db_->InsertContent(url.spec(), proto,
                             base::BindOnce([](bool succeeded) {}));
  } else {
    DeleteDiscountsForUrl(url.spec());
  }
}

void DiscountInfosStorage::DeleteDiscountsForUrl(const std::string& url) {
  proto_db_->DeleteOneEntry(url, base::BindOnce([](bool succeeded) {}));
}

void DiscountInfosStorage::OnLoadDiscounts(const GURL& url,
                                           DiscountInfoCallback callback,
                                           bool succeeded,
                                           DiscountInfosKeyAndValues data) {
  if (!succeeded || data.size() == 0) {
    std::move(callback).Run(url, {});
    return;
  }
  std::vector<DiscountInfo> unexpired_infos;
  for (const auto& [key, value] : data) {
    std::vector<DiscountInfo> valid_infos =
        GetUnexpiredDiscountsFromProto(value);

    if (valid_infos.size() == 0) {
      // Delete the entry in the db if no unexpired discounts found.
      DeleteDiscountsForUrl(key);
    } else {
      // Update local database if expired discounts found.
      if ((int)(valid_infos.size()) != value.discounts().size()) {
        SaveDiscounts(GURL(key), valid_infos);
      }
      unexpired_infos.insert(unexpired_infos.end(), valid_infos.begin(),
                             valid_infos.end());
    }
  }
  std::vector<DiscountInfo> unique_infos =
      RemoveDuplicateDiscountsFromProto(unexpired_infos);
  std::move(callback).Run(url, std::move(unique_infos));
}

std::vector<DiscountInfo> DiscountInfosStorage::GetUnexpiredDiscountsFromProto(
    const DiscountInfosContent& proto) {
  std::vector<DiscountInfo> infos;
  for (const discount_infos_db::DiscountInfoContent& content :
       proto.discounts()) {
    // First check whether the discount is expired.
    if ((base::Time::Now() - base::Time::UnixEpoch()).InSeconds() >
        content.expiry_time_sec()) {
      continue;
    }

    DiscountInfo info;
    info.id = content.id();
    info.type = DiscountType(content.type());
    info.language_code = content.language_code();
    info.description_detail = content.description_detail();
    if (content.has_terms_and_conditions()) {
      info.terms_and_conditions = content.terms_and_conditions();
    }
    info.expiry_time_sec = content.expiry_time_sec();
    if (content.has_discount_code()) {
      info.discount_code = content.discount_code();
    }
    info.offer_id = content.offer_id();
    infos.push_back(info);
  }

  return infos;
}

std::vector<DiscountInfo>
DiscountInfosStorage::RemoveDuplicateDiscountsFromProto(
    const std::vector<DiscountInfo>& infos) {
  std::vector<DiscountInfo> unique_infos;
  std::map<std::string, bool> existing_codes;

  for (const DiscountInfo& info : infos) {
    DCHECK(info.discount_code.has_value());
    const std::string& discount_code = info.discount_code.value();
    // Check if the discount code is already present in the map.
    if (!existing_codes.contains(discount_code)) {
      // If not present, add the discount code to the map and to the result
      // vector.
      existing_codes[discount_code] = true;
      unique_infos.push_back(info);
    }
  }
  return unique_infos;
}

void DiscountInfosStorage::OnHistoryDeletions(
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
