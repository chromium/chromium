// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_DELETION_ORIGIN_H_
#define COMPONENTS_SYNC_BASE_DELETION_ORIGIN_H_

#include <optional>
#include <string_view>

#include "base/location.h"

namespace sync_pb {
class DeletionOrigin;
}  // namespace sync_pb

namespace syncer {

// Represents a fingerprint-like token that identifies, or may help identify,
// which piece of functionality is responsible for issuing a deletion that
// propagates via Sync. It is sent to the Sync server as part of deletion
// requests, as a safeguard to investigate and mitigate user reports or even
// large-scale incidents.
class DeletionOrigin {
 public:
  static DeletionOrigin Unspecified();
  static DeletionOrigin FromLocation(base::Location location);

  DeletionOrigin(const DeletionOrigin& other);
  DeletionOrigin(DeletionOrigin&& other);
  ~DeletionOrigin();

  DeletionOrigin& operator=(const DeletionOrigin& other);
  DeletionOrigin& operator=(DeletionOrigin&& other);

  // Returns true if this origin is non-empty (aka specified).
  bool is_specified() const;

  // Converts to a serializable protocol buffer. Must only be called if
  // `is_specified()` is true.
  sync_pb::DeletionOrigin ToProto(std::string_view chromium_version) const;

  // Test-only API to allow comparing with a base::Location.
  const std::optional<base::Location>& GetLocationForTesting() const {
    return location_;
  }

 private:
  explicit DeletionOrigin(std::optional<base::Location> location);
  std::optional<base::Location> location_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_DELETION_ORIGIN_H_
