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

}  // namespace commerce
