// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_BLOCKLIST_DATA_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_BLOCKLIST_DATA_H_

#include <string>

#include "base/sequence_checker.h"
#include "third_party/re2/src/re2/re2.h"

namespace plus_addresses {

// This holds the set of patterns that define the set of facets for which Plus
// Addresses should not be offered. The data to populate it is read from the
// Component Updater, which fetches it periodically from Google to get the
// most up-to-date patterns.
class PlusAddressBlocklistData final {
 public:
  static PlusAddressBlocklistData& GetInstance();

  PlusAddressBlocklistData();
  PlusAddressBlocklistData(const PlusAddressBlocklistData&) = delete;
  PlusAddressBlocklistData& operator=(const PlusAddressBlocklistData&) = delete;
  ~PlusAddressBlocklistData();

  // Updates the internal list from a binary proto fetched from the network.
  // This can be called multiple times with new protos. Returns `true` if the
  // parsing process was successful, `false` otherwise.
  bool PopulateDataFromComponent(const std::string& binary_pb);

  // Returns a regular expression that specifies which domains should not have
  // Plus Addresses offered on them.
  const re2::RE2* GetExclusionPattern() const;

  // Returns a regular expression that specifies which domains should be exempt
  // from a blocklist rule.
  const re2::RE2* GetExceptionPattern() const;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<re2::RE2> exclusion_pattern_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<re2::RE2> exception_pattern_
      GUARDED_BY_CONTEXT(sequence_checker_);
};
}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_BLOCKLIST_DATA_H_
