// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_DISCOUNTS_STORAGE_H_
#define COMPONENTS_COMMERCE_CORE_DISCOUNTS_STORAGE_H_

#include "base/check.h"
#include "base/functional/callback.h"
#include "components/commerce/core/proto/discounts_db_content.pb.h"
#include "components/session_proto_db/session_proto_storage.h"

namespace commerce {

using DiscountsContent = discounts_db::DiscountsContentProto;

class DiscountsStorage {
 public:
  explicit DiscountsStorage(
      SessionProtoStorage<DiscountsContent>* discounts_proto_db);
  DiscountsStorage(const DiscountsStorage&) = delete;
  DiscountsStorage& operator=(const DiscountsStorage&) = delete;
  virtual ~DiscountsStorage();

 private:
  raw_ptr<SessionProtoStorage<DiscountsContent>> proto_db_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_DISCOUNTS_STORAGE_H_
