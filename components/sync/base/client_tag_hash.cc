// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/client_tag_hash.h"

#include <utility>

#include "base/base64.h"
#include "base/hash/sha1.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/protocol/entity_specifics.pb.h"

namespace syncer {

// static
ClientTagHash ClientTagHash::FromUnhashed(DataType data_type,
                                          std::string_view client_tag) {
  // Blank PB with just the field in it has termination symbol,
  // handy for delimiter.
  sync_pb::EntitySpecifics serialized_type;
  AddDefaultFieldValue(data_type, &serialized_type);
  std::string hash_input;
  serialized_type.AppendToString(&hash_input);
  hash_input.append(client_tag);

  return FromHashed(
      base::Base64Encode(base::SHA1Hash(base::as_byte_span(hash_input))));
}

// static
ClientTagHash ClientTagHash::FromHashed(std::string hash_value) {
  return ClientTagHash(std::move(hash_value));
}

ClientTagHash::ClientTagHash() = default;

ClientTagHash::ClientTagHash(std::string value) : value_(std::move(value)) {}

ClientTagHash::ClientTagHash(const ClientTagHash& other) = default;

ClientTagHash& ClientTagHash::operator=(const ClientTagHash& other) = default;

ClientTagHash::ClientTagHash(ClientTagHash&& other) = default;

ClientTagHash& ClientTagHash::operator=(ClientTagHash&& other) = default;

ClientTagHash::~ClientTagHash() = default;

size_t ClientTagHash::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(value_);
}

std::ostream& operator<<(std::ostream& os,
                         const ClientTagHash& client_tag_hash) {
  return os << client_tag_hash.value();
}

}  // namespace syncer
