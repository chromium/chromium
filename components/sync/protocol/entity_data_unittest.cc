// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/entity_data.h"

#include "base/test/protobuf_matchers.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/attachment.pb.h"
#include "components/sync/protocol/attachment_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using base::test::EqualsProto;

class EntityDataTest : public testing::Test {
 protected:
  EntityDataTest() = default;
  ~EntityDataTest() override = default;
};

TEST_F(EntityDataTest, IsDeleted) {
  EntityData data;
  EXPECT_TRUE(data.is_deleted());

  AddDefaultFieldValue(BOOKMARKS, &data.specifics);
  EXPECT_FALSE(data.is_deleted());
}

// Verifies that `AttachmentMetadata::FromProto()` and `ToProto()` round-trip
// `sync_pb::Attachment`.
TEST_F(EntityDataTest, AttachmentMetadataFromAndToProto) {
  sync_pb::Attachment proto;
  proto.set_temporary_blob_id("temp_blob_123");
  EXPECT_THAT(AttachmentMetadata::FromProto(proto).ToProto(),
              EqualsProto(proto));
}

}  // namespace syncer
