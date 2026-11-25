// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/attachment_metadata.h"

#include <optional>

#include "base/test/protobuf_matchers.h"
#include "base/time/time.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/attachment.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using base::test::EqualsProto;
using testing::IsEmpty;
using testing::Optional;

// Verifies round-tripping a new attachment carrying `temporary_blob_id`.
TEST(AttachmentMetadataTest, FromAndToProtoWithTemporaryBlobId) {
  sync_pb::Attachment proto;
  proto.set_temporary_blob_id("temp_blob_123");

  AttachmentMetadata metadata = AttachmentMetadata::FromProto(proto);
  EXPECT_EQ(metadata.temporary_blob_id(), "temp_blob_123");
  EXPECT_EQ(metadata.server_provided_modification_time(), std::nullopt);
  EXPECT_EQ(metadata, AttachmentMetadata::ForNew("temp_blob_123"));
  EXPECT_THAT(metadata.ToProto(), EqualsProto(proto));
}

// Verifies round-tripping a timestamp-only attachment without a blob ID.
TEST(AttachmentMetadataTest, FromAndToProtoWithModificationTime) {
  const base::Time modification_time = ProtoTimeToTime(987654321);
  sync_pb::Attachment proto;
  proto.set_modification_time_unix_epoch_millis(987654321);

  AttachmentMetadata metadata = AttachmentMetadata::FromProto(proto);
  EXPECT_THAT(metadata.temporary_blob_id(), IsEmpty());
  EXPECT_THAT(metadata.server_provided_modification_time(),
              Optional(modification_time));
  EXPECT_THAT(metadata.ToProto(), EqualsProto(proto));
}

// Verifies round-tripping an empty `sync_pb::Attachment` proto.
TEST(AttachmentMetadataTest, FromAndToProtoEmpty) {
  sync_pb::Attachment proto;

  AttachmentMetadata metadata = AttachmentMetadata::FromProto(proto);
  EXPECT_THAT(metadata.temporary_blob_id(), IsEmpty());
  EXPECT_EQ(metadata.server_provided_modification_time(), std::nullopt);
  EXPECT_THAT(metadata.ToProto(), EqualsProto(proto));
}

// Verifies round-tripping an attachment proto with both fields populated.
TEST(AttachmentMetadataTest, FromAndToProtoWithBothFields) {
  sync_pb::Attachment proto;
  proto.set_temporary_blob_id("temp_blob_123");
  proto.set_modification_time_unix_epoch_millis(987654321);

  AttachmentMetadata metadata = AttachmentMetadata::FromProto(proto);
  EXPECT_EQ(metadata.temporary_blob_id(), "temp_blob_123");
  EXPECT_THAT(metadata.server_provided_modification_time(),
              Optional(ProtoTimeToTime(987654321)));
  EXPECT_THAT(metadata.ToProto(), EqualsProto(proto));
}

}  // namespace
}  // namespace syncer
