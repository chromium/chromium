// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_CLIENT_TAG_HASH_H_
#define COMPONENTS_SYNC_BASE_CLIENT_TAG_HASH_H_

#include <compare>
#include <iosfwd>
#include <string>
#include <string_view>

#include "base/hash/hash.h"
#include "components/sync/base/data_type.h"

namespace syncer {

// Represents a client defined unique hash for sync entities. Hash is derived
// from client tag, and should be used as |client_tag_hash| for
// SyncEntity at least for CommitMessages. For convenience it supports storing
// in ordered stl containers, logging and equality comparisons. It also supports
// unordered stl containers using ClientTagHash::Hash.
class ClientTagHash {
 public:
  // For use in std::unordered_map.
  struct Hash {
    size_t operator()(const ClientTagHash& client_tag_hash) const {
      return base::FastHash(client_tag_hash.value());
    }
  };

  // Creates ClientTagHash based on |client_tag|.
  static ClientTagHash FromUnhashed(DataType type, std::string_view client_tag);

  // Creates ClientTagHash from already hashed client tag.
  static ClientTagHash FromHashed(std::string hash_value);

  ClientTagHash();
  ClientTagHash(const ClientTagHash& other);
  ClientTagHash& operator=(const ClientTagHash& other);
  ClientTagHash(ClientTagHash&& other);
  ClientTagHash& operator=(ClientTagHash&& other);
  ~ClientTagHash();

  friend bool operator==(const ClientTagHash&, const ClientTagHash&) = default;
  friend std::strong_ordering operator<=>(const ClientTagHash&,
                                          const ClientTagHash&) = default;

  const std::string& value() const { return value_; }

  size_t EstimateMemoryUsage() const;

 private:
  explicit ClientTagHash(std::string value);
  std::string value_;
};

std::ostream& operator<<(std::ostream& os,
                         const ClientTagHash& client_tag_hash);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_CLIENT_TAG_HASH_H_
