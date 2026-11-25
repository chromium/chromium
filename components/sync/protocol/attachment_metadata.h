// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_ATTACHMENT_METADATA_H_
#define COMPONENTS_SYNC_PROTOCOL_ATTACHMENT_METADATA_H_

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>

#include "base/time/time.h"

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

  // Constructs an `AttachmentMetadata` from the given `proto`.
  static AttachmentMetadata FromProto(const sync_pb::Attachment& proto);

  AttachmentMetadata(const AttachmentMetadata& other);
  AttachmentMetadata& operator=(const AttachmentMetadata& other);
  AttachmentMetadata(AttachmentMetadata&& other);
  AttachmentMetadata& operator=(AttachmentMetadata&& other);
  ~AttachmentMetadata();

  // The temporary blob ID assigned when uploading a new attachment (may be
  // empty).
  const std::string& temporary_blob_id() const { return temporary_blob_id_; }

  // The server-assigned modification time of an existing attachment (if set).
  std::optional<base::Time> server_provided_modification_time() const {
    return server_provided_modification_time_;
  }

  sync_pb::Attachment ToProto() const;

  size_t EstimateMemoryUsage() const;

  friend bool operator==(const AttachmentMetadata&,
                         const AttachmentMetadata&) = default;

 private:
  friend void PrintTo(const AttachmentMetadata& attachment_metadata,
                      std::ostream* os);

  AttachmentMetadata(
      std::string temporary_blob_id,
      std::optional<base::Time> server_provided_modification_time);

  // Populated when committing a newly uploaded attachment to the Sync server.
  std::string temporary_blob_id_;
  // Populated for an already-committed attachment with its server-assigned
  // modification time.
  std::optional<base::Time> server_provided_modification_time_;
};

// gMock printer helper.
void PrintTo(const AttachmentMetadata& attachment_metadata, std::ostream* os);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_PROTOCOL_ATTACHMENT_METADATA_H_
