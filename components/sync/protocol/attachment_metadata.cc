// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/attachment_metadata.h"

#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/attachment.pb.h"

namespace syncer {

// static
AttachmentMetadata AttachmentMetadata::ForNew(std::string temporary_blob_id) {
  CHECK(!temporary_blob_id.empty());
  return AttachmentMetadata(std::move(temporary_blob_id),
                            /*server_provided_modification_time=*/std::nullopt);
}

// static
AttachmentMetadata AttachmentMetadata::FromProto(
    const sync_pb::Attachment& proto) {
  std::optional<base::Time> server_provided_modification_time;
  if (proto.has_modification_time_unix_epoch_millis()) {
    server_provided_modification_time =
        ProtoTimeToTime(proto.modification_time_unix_epoch_millis());
  }
  return AttachmentMetadata(proto.temporary_blob_id(),
                            server_provided_modification_time);
}

AttachmentMetadata::AttachmentMetadata(const AttachmentMetadata& other) =
    default;
AttachmentMetadata& AttachmentMetadata::operator=(
    const AttachmentMetadata& other) = default;
AttachmentMetadata::AttachmentMetadata(AttachmentMetadata&& other) = default;
AttachmentMetadata& AttachmentMetadata::operator=(AttachmentMetadata&& other) =
    default;
AttachmentMetadata::~AttachmentMetadata() = default;

sync_pb::Attachment AttachmentMetadata::ToProto() const {
  sync_pb::Attachment proto;
  if (!temporary_blob_id_.empty()) {
    proto.set_temporary_blob_id(temporary_blob_id_);
  }
  if (server_provided_modification_time_.has_value()) {
    proto.set_modification_time_unix_epoch_millis(
        TimeToProtoTime(*server_provided_modification_time_));
  }
  return proto;
}

size_t AttachmentMetadata::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(temporary_blob_id_);
}

AttachmentMetadata::AttachmentMetadata(
    std::string temporary_blob_id,
    std::optional<base::Time> server_provided_modification_time)
    : temporary_blob_id_(std::move(temporary_blob_id)),
      server_provided_modification_time_(server_provided_modification_time) {}

void PrintTo(const AttachmentMetadata& attachment_metadata, std::ostream* os) {
  *os << "{ temporary_blob_id: '" << attachment_metadata.temporary_blob_id_
      << "', server_provided_modification_time: ";
  if (attachment_metadata.server_provided_modification_time_.has_value()) {
    *os << *attachment_metadata.server_provided_modification_time_;
  } else {
    *os << "nullopt";
  }
  *os << " }";
}

}  // namespace syncer
