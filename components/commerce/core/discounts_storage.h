// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_DISCOUNTS_STORAGE_H_
#define COMPONENTS_COMMERCE_CORE_DISCOUNTS_STORAGE_H_

#include <map>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_observation.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/proto/discounts_db_content.pb.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/session_proto_db/session_proto_storage.h"

namespace commerce {

using DiscountsContent = discounts_db::DiscountsContentProto;
using DiscountsKeyAndValues =
    std::vector<SessionProtoStorage<DiscountsContent>::KeyAndValue>;

extern const char kDiscountsFetchResultHistogramName[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DiscountsFetchResult {
  // We got info back from OptGuide.
  kInfoFromOptGuide = 0,
  // We found valid info in local db. This is only recorded when we don't get
  // info back from OptGuide.
  kValidInfoInDb = 1,
  // We found invalid info in local db. This is only recorded when we don't get
  // info back from OptGuide.
  kInvalidInfoInDb = 2,
  // We don't get info back from OptGuide and don't find any info in local db.
  kInfoNotFound = 3,

  kMaxValue = kInfoNotFound
};

class DiscountsStorage : public history::HistoryServiceObserver {
 public:
  explicit DiscountsStorage(
      SessionProtoStorage<DiscountsContent>* discounts_proto_db,
      history::HistoryService* history_service);
  DiscountsStorage(const DiscountsStorage&) = delete;
  DiscountsStorage& operator=(const DiscountsStorage&) = delete;
  ~DiscountsStorage() override;

  virtual void HandleServerDiscounts(const GURL& url,
                                     std::vector<DiscountInfo> server_results,
                                     DiscountInfoCallback callback);

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

 private:
  void SaveDiscounts(const GURL& url, const std::vector<DiscountInfo>& infos);

  void DeleteDiscountsForUrl(const std::string& url);

  void OnLoadDiscounts(const GURL& url,
                       DiscountInfoCallback callback,
                       bool succeeded,
                       DiscountsKeyAndValues data);

  // When loading from local db, discard expired discounts and only convert &
  // return unexpired ones.
  std::vector<DiscountInfo> GetUnexpiredDiscountsFromProto(
      const DiscountsContent& proto);

  raw_ptr<SessionProtoStorage<DiscountsContent>> proto_db_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<DiscountsStorage> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_DISCOUNTS_STORAGE_H_
