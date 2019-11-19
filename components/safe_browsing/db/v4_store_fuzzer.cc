// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/test/test_simple_task_runner.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/db/v4_store.h"
#include "components/safe_browsing/db/v4_test_util.h"

namespace safe_browsing {

const PrefixSize kMinHashPrefixLengthForFuzzing = kMinHashPrefixLength;
const PrefixSize kMaxHashPrefixLengthForFuzzing = 8;

class V4StoreFuzzer {
 public:
  static int FuzzMergeUpdate(const uint8_t* data, size_t size) {
    // |prefix_map_old| represents the existing state of the |V4Store|.
    HashPrefixMap prefix_map_old;
    // |prefix_map_additions| represents the update being applied.
    HashPrefixMap prefix_map_additions;

    // Pass 1:
    // Add a prefix_size->[prefixes] pair in |prefix_map_old|.
    PopulateHashPrefixMap(&data, &size, &prefix_map_old);
    // Add a prefix_size->[prefixes] pair in |prefix_map_additions|.
    PopulateHashPrefixMap(&data, &size, &prefix_map_additions);

    // Pass 2:
    // Add a prefix_size->[prefixes] pair in |prefix_map_old|.
    // If the prefix_size is the same as that added in |prefix_map_old| during
    // Pass 1, the older list of prefixes is lost.
    PopulateHashPrefixMap(&data, &size, &prefix_map_old);
    // Add a prefix_size->[prefixes] pair in |prefix_map_additions|.
    // If the prefix_size is the same as that added in |prefix_map_additions|
    // during Pass 1, the older list of prefixes is lost.
    PopulateHashPrefixMap(&data, &size, &prefix_map_additions);

    auto store = std::make_unique<TestV4Store>(
        base::MakeRefCounted<base::TestSimpleTaskRunner>(), base::FilePath());
    // Assume no removals.
    google::protobuf::RepeatedField<google::protobuf::int32> raw_removals;
    // Empty checksum indicates that the checksum calculation should be skipped.
    std::string empty_checksum;
    store->MergeUpdate(prefix_map_old, prefix_map_additions, &raw_removals,
                       empty_checksum);
#ifndef NDEBUG
    DisplayHashPrefixMapDetails(store->hash_prefix_map_);
#endif

    return 0;
  }

 private:
  // Add a prefix_size->[prefixes] pair in |hash_prefix_map|.
  // Ensures that length of [prefixes] is a multiple of prefix_size.
  // If the map already contains a pair with key prefix_size, the existing value
  // is discarded.
  // Here's a summary of how the input is parsed:
  // * First uint8_t is the |prefix_size| to be added.
  // * Next uint8_t is the length of the list of prefixes.
  //  * It is adjusted to be no greater than the remaining size of |data|.
  //  * It is called as |prefixes_list_size|.
  // * Next |prefixes_list_size| bytes are added to |hash_prefix_map|
  //   as a list of prefixes of size |prefix_size|.
  static void PopulateHashPrefixMap(const uint8_t** data,
                                    size_t* size,
                                    HashPrefixMap* hash_prefix_map) {
    uint8_t datum;
    if (!GetDatum(data, size, &datum))
      return;

    // Prefix size is defined to be between |kMinHashPrefixLength| and
    // |kMaxHashPrefixLength| but we are going to limit them to smaller sizes so
    // that we have a higher chance of actually populating the
    // |hash_prefix_map| for smaller inputs.
    PrefixSize prefix_size = kMinHashPrefixLengthForFuzzing +
                             (datum % (kMaxHashPrefixLengthForFuzzing -
                                       kMinHashPrefixLengthForFuzzing + 1));

    if (!GetDatum(data, size, &datum))
      return;
    size_t prefixes_list_size = datum;
    // This |prefixes_list_size| is the length of the list of prefixes to be
    // added. It can't be larger than the remaining buffer.
    if (*size < prefixes_list_size) {
      prefixes_list_size = *size;
    }
    std::string prefixes(*data, *data + prefixes_list_size);
    *size -= prefixes_list_size;
    *data += prefixes_list_size;
    V4Store::AddUnlumpedHashes(prefix_size, prefixes, hash_prefix_map);
#ifndef NDEBUG
    DisplayHashPrefixMapDetails(*hash_prefix_map);
#endif
  }

  static bool GetDatum(const uint8_t** data, size_t* size, uint8_t* datum) {
    if (*size == 0)
      return false;
    *datum = *data[0];
    (*data)++;
    (*size)--;
    return true;
  }

  static void DisplayHashPrefixMapDetails(
      const HashPrefixMap& hash_prefix_map) {
    for (const auto& pair : hash_prefix_map) {
      PrefixSize prefix_size = pair.first;
      size_t prefixes_length = pair.second.length();
      DVLOG(5) << __FUNCTION__ << " : " << prefix_size << " : "
               << prefixes_length;
    }
  }
};

}  // namespace safe_browsing

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return safe_browsing::V4StoreFuzzer::FuzzMergeUpdate(data, size);
}
