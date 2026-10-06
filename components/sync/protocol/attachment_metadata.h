// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_ATTACHMENT_METADATA_H_
#define COMPONENTS_SYNC_PROTOCOL_ATTACHMENT_METADATA_H_

#include <cstddef>
#include <iosfwd>
#include <string>

namespace sync_pb {
class Attachment;
}  // namespace sync_pb

namespace syncer {

// Metadata associated with a blob attachment referenced by a Sync entity.
class AttachmentMetadata {
 public:
  // Creates an `AttachmentMetadata` with a non-empty `temporary_blob_id` ready
  // to be committed to the Sync server.
  static AttachmentMetadata ForNew(std::string temporary_blob_id);

  AttachmentMetadata(const AttachmentMetadata& other);
  AttachmentMetadata& operator=(const AttachmentMetadata& other);
  AttachmentMetadata(AttachmentMetadata&& other);
  AttachmentMetadata& operator=(AttachmentMetadata&& other);
  ~AttachmentMetadata();

  sync_pb::Attachment ToProto() const;

  size_t EstimateMemoryUsage() const;

  friend bool operator==(const AttachmentMetadata&,
                         const AttachmentMetadata&) = default;

 private:
  friend void PrintTo(const AttachmentMetadata& attachment_metadata,
                      std::ostream* os);

  explicit AttachmentMetadata(std::string temporary_blob_id);

  // Populated when committing a newly uploaded attachment to the Sync server.
  std::string temporary_blob_id_;
};

// gMock printer helper.
void PrintTo(const AttachmentMetadata& attachment_metadata, std::ostream* os);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_PROTOCOL_ATTACHMENT_METADATA_H_
