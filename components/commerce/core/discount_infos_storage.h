// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_DISCOUNT_INFOS_STORAGE_H_
#define COMPONENTS_COMMERCE_CORE_DISCOUNT_INFOS_STORAGE_H_

#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_observation.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/proto/discount_infos_db_content.pb.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/session_proto_db/session_proto_storage.h"

namespace commerce {

using DiscountInfosContent = discount_infos_db::DiscountInfosContentProto;
using DiscountInfosKeyAndValues =
    std::vector<SessionProtoStorage<DiscountInfosContent>::KeyAndValue>;

class DiscountInfosStorage : public history::HistoryServiceObserver {
 public:
  explicit DiscountInfosStorage(
      SessionProtoStorage<DiscountInfosContent>* discount_infos_proto_db,
      history::HistoryService* history_service);
  DiscountInfosStorage(const DiscountInfosStorage&) = delete;
  DiscountInfosStorage& operator=(const DiscountInfosStorage&) = delete;
  ~DiscountInfosStorage() override;

  // Load all discounts with prefix matching the given url.
  virtual void LoadDiscountsWithPrefix(const GURL& url,
                                       DiscountInfoCallback callback);

  // Save discounts for the given url.
  virtual void SaveDiscounts(const GURL& url,
                             const std::vector<DiscountInfo>& infos);

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

 private:
  void DeleteDiscountsForUrl(const std::string& url);

  void OnLoadDiscounts(const GURL& url,
                       DiscountInfoCallback callback,
                       bool succeeded,
                       DiscountInfosKeyAndValues data);

  // When loading from local db, discard expired discounts and only convert &
  // return unexpired ones.
  std::vector<DiscountInfo> GetUnexpiredDiscountsFromProto(
      const DiscountInfosContent& proto);

  // When loading from local db, remove duplicate discounts that have the same
  // discount code.
  std::vector<DiscountInfo> RemoveDuplicateDiscountsFromProto(
      const std::vector<DiscountInfo>& infos);

  raw_ptr<SessionProtoStorage<DiscountInfosContent>> proto_db_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<DiscountInfosStorage> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_DISCOUNT_INFOS_STORAGE_H_
