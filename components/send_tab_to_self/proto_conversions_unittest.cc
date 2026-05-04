// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/proto_conversions.h"

#include <optional>

#include "components/autofill/core/browser/field_types.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace send_tab_to_self {
namespace {

TEST(SendTabToSelfProtoConversionsTest, AutofillFieldTypeToProto_FillableType) {
  EXPECT_EQ(AutofillFieldTypeToProto(autofill::EMAIL_ADDRESS),
            sync_pb::FormField_AutofillFieldType_EMAIL_ADDRESS);
  EXPECT_EQ(AutofillFieldTypeToProto(autofill::USERNAME),
            sync_pb::FormField_AutofillFieldType_USERNAME);
}

TEST(SendTabToSelfProtoConversionsTest,
     AutofillFieldTypeToProto_NonFillableType) {
  EXPECT_EQ(AutofillFieldTypeToProto(autofill::UNKNOWN_TYPE), std::nullopt);
  EXPECT_EQ(AutofillFieldTypeToProto(autofill::MERCHANT_EMAIL_SIGNUP),
            std::nullopt);
}

}  // namespace
}  // namespace send_tab_to_self
