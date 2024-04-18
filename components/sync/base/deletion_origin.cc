// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/deletion_origin.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "components/sync/protocol/deletion_origin.pb.h"

namespace syncer {
namespace {

constexpr size_t kMaxFileNameBeforeTruncation = 30;

// Truncates the filename to a maximum size by stripping, if needed, the
// beginning of the string (usually path), which is less representative than the
// end.
std::string MaybeTruncateFileName(std::string_view file_name) {
  if (file_name.size() <= kMaxFileNameBeforeTruncation) {
    return std::string(file_name);
  }

  std::string result(
      file_name.substr(file_name.size() - kMaxFileNameBeforeTruncation));
  for (int i = 0; i < 3; i++) {
    result[i] = '.';
  }
  return result;
}

}  // namespace

// static
DeletionOrigin DeletionOrigin::Unspecified() {
  return DeletionOrigin(std::nullopt);
}

// static
DeletionOrigin DeletionOrigin::FromLocation(base::Location location) {
  return DeletionOrigin(std::move(location));
}

DeletionOrigin::DeletionOrigin(const DeletionOrigin& other) = default;

DeletionOrigin::DeletionOrigin(DeletionOrigin&& other) = default;

DeletionOrigin::~DeletionOrigin() = default;

DeletionOrigin& DeletionOrigin::operator=(const DeletionOrigin& other) =
    default;

DeletionOrigin& DeletionOrigin::operator=(DeletionOrigin&& other) = default;

bool DeletionOrigin::is_specified() const {
  return location_.has_value();
}

sync_pb::DeletionOrigin DeletionOrigin::ToProto(
    std::string_view chromium_version) const {
  CHECK(is_specified());

  sync_pb::DeletionOrigin proto;
  proto.set_chromium_version(std::string(chromium_version));
  proto.set_file_name_hash(base::PersistentHash((location_->file_name())));
  proto.set_file_line_number(location_->line_number());
  if (base::IsStringASCII(location_->file_name())) {
    proto.set_file_name_possibly_truncated(
        MaybeTruncateFileName(location_->file_name()));
  }
  return proto;
}

DeletionOrigin::DeletionOrigin(std::optional<base::Location> location)
    : location_(std::move(location)) {}

}  // namespace syncer
