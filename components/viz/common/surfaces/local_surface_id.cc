// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/local_surface_id.h"

#include <limits>

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace viz {

size_t LocalSurfaceId::hash() const {
  DCHECK(is_valid()) << ToString();
  return base::HashInts(
      static_cast<uint64_t>(
          base::HashInts(parent_sequence_number_, child_sequence_number_)),
      static_cast<uint64_t>(base::UnguessableTokenHash()(embed_token_)));
}

size_t LocalSurfaceId::persistent_hash() const {
  DCHECK(is_valid()) << ToString();
  return base::PersistentHash(
      base::StringPrintf("%s, %u, %u", embed_token_.ToString().c_str(),
                         parent_sequence_number_, child_sequence_number_));
}

std::string LocalSurfaceId::ToString() const {
  std::string embed_token = VLOG_IS_ON(1)
                                ? embed_token_.ToString()
                                : embed_token_.ToString().substr(0, 4) + "...";

  return base::StringPrintf("LocalSurfaceId(%u, %u, %s)",
                            parent_sequence_number_, child_sequence_number_,
                            embed_token.c_str());
}

std::ostream& operator<<(std::ostream& out,
                         const LocalSurfaceId& local_surface_id) {
  return out << local_surface_id.ToString();
}

bool LocalSurfaceId::IsNewerThan(const LocalSurfaceId& other) const {
  // Sequence numbers can wrap around so look at their difference instead of
  // their absolute values.
  return embed_token_ == other.embed_token_ &&
         (child_sequence_number_ - other.child_sequence_number_ < (1u << 31)) &&
         (parent_sequence_number_ - other.parent_sequence_number_ <
          (1u << 31)) &&
         (child_sequence_number_ != other.child_sequence_number_ ||
          parent_sequence_number_ != other.parent_sequence_number_);
}

bool LocalSurfaceId::IsNewerThanOrEmbeddingChanged(
    const LocalSurfaceId& other) const {
  return IsNewerThan(other) || embed_token_ != other.embed_token_;
}

bool LocalSurfaceId::IsSameOrNewerThan(const LocalSurfaceId& other) const {
  return IsNewerThan(other) || *this == other;
}

LocalSurfaceId LocalSurfaceId::ToSmallestId() const {
  return LocalSurfaceId(1, 1, embed_token_);
}

void LocalSurfaceId::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto->set_parent_sequence_number(parent_sequence_number_);
  proto->set_child_sequence_number(child_sequence_number_);
  perfetto::protos::pbzero::ChromeUnguessableToken& unguessable_token =
      *(proto->set_unguessable_token());
  unguessable_token.set_low_token(embed_token_.GetLowForSerialization());
  unguessable_token.set_high_token(embed_token_.GetHighForSerialization());
}

}  // namespace viz
