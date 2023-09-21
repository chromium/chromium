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
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/proto/discounts_db_content.pb.h"
#include "components/session_proto_db/session_proto_storage.h"

namespace commerce {

using DiscountsContent = discounts_db::DiscountsContentProto;
using DiscountsKeyAndValues =
    std::vector<SessionProtoStorage<DiscountsContent>::KeyAndValue>;

class DiscountsStorage {
 public:
  explicit DiscountsStorage(
      SessionProtoStorage<DiscountsContent>* discounts_proto_db);
  DiscountsStorage(const DiscountsStorage&) = delete;
  DiscountsStorage& operator=(const DiscountsStorage&) = delete;
  virtual ~DiscountsStorage();

  virtual void HandleServerDiscounts(
      const std::vector<std::string>& urls_to_check,
      DiscountsMap server_results,
      DiscountInfoCallback callback);

 private:
  void SaveDiscounts(const GURL& url, const std::vector<DiscountInfo>& infos);

  void DeleteDiscountsForUrl(const std::string& url);

  void OnLoadAllDiscounts(const std::vector<std::string>& urls_to_check,
                          DiscountsMap server_results,
                          DiscountInfoCallback callback,
                          bool succeeded,
                          DiscountsKeyAndValues data);

  // When loading from local db, discard expired discounts and only convert &
  // return unexpired ones.
  std::vector<DiscountInfo> GetUnexpiredDiscountsFromProto(
      const DiscountsContent& proto);

  raw_ptr<SessionProtoStorage<DiscountsContent>> proto_db_;

  base::WeakPtrFactory<DiscountsStorage> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_DISCOUNTS_STORAGE_H_
