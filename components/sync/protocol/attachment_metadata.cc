// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/attachment_metadata.h"

#include <cstddef>
#include <ostream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/protocol/sync_entity.pb.h"

namespace syncer {

// static
AttachmentMetadata AttachmentMetadata::ForNew(std::string temporary_blob_id) {
  CHECK(!temporary_blob_id.empty());
  return AttachmentMetadata(std::move(temporary_blob_id));
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
  return proto;
}

size_t AttachmentMetadata::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(temporary_blob_id_);
}

AttachmentMetadata::AttachmentMetadata(std::string temporary_blob_id)
    : temporary_blob_id_(std::move(temporary_blob_id)) {}

void PrintTo(const AttachmentMetadata& attachment_metadata, std::ostream* os) {
  *os << "{ temporary_blob_id: '" << attachment_metadata.temporary_blob_id_
      << "'}";
}

}  // namespace syncer
