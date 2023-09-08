// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/discounts_storage.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/proto/discounts_db_content.pb.h"
#include "components/session_proto_db/session_proto_storage.h"

namespace commerce {

DiscountsStorage::DiscountsStorage(
    SessionProtoStorage<DiscountsContent>* discounts_proto_db)
    : proto_db_(discounts_proto_db) {}
DiscountsStorage::~DiscountsStorage() = default;

}  // namespace commerce
