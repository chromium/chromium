// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v4_store.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/test_simple_task_runner.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"

namespace safe_browsing {

const PrefixSize kMinHashPrefixLengthForFuzzing = kMinHashPrefixLength;
const PrefixSize kMaxHashPrefixLengthForFuzzing = 8;

class V4StoreFuzzer {
 public:
  static int FuzzMergeUpdate(base::span<const uint8_t> data) {
    // |prefix_map_old| represents the existing state of the |V4Store|.
    std::unordered_map<PrefixSize, HashPrefixes> prefix_map_old;
    // |prefix_map_additions| represents the update being applied.
    std::unordered_map<PrefixSize, HashPrefixes> prefix_map_additions;

    // Pass 1:
    // Add a prefix_size->[prefixes] pair in |prefix_map_old|.
    PopulateHashPrefixMap(&data, &prefix_map_old);
    // Add a prefix_size->[prefixes] pair in |prefix_map_additions|.
    PopulateHashPrefixMap(&data, &prefix_map_additions);

    // Pass 2:
    // Add a prefix_size->[prefixes] pair in |prefix_map_old|.
    // If the prefix_size is the same as that added in |prefix_map_old| during
    // Pass 1, the older list of prefixes is lost.
    PopulateHashPrefixMap(&data, &prefix_map_old);
    // Add a prefix_size->[prefixes] pair in |prefix_map_additions|.
    // If the prefix_size is the same as that added in |prefix_map_additions|
    // during Pass 1, the older list of prefixes is lost.
    PopulateHashPrefixMap(&data, &prefix_map_additions);

    base::ScopedTempDir temp_dir;
    CHECK(temp_dir.CreateUniqueTempDir());
    base::FilePath store_path =
        temp_dir.GetPath().AppendASCII("V4StoreTest.store");

    auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    auto store = std::make_unique<V4Store>(task_runner, store_path);
    // Assume no removals.
    google::protobuf::RepeatedField<google::protobuf::int32> raw_removals;
    // Empty checksum indicates that the checksum calculation should be skipped.
    std::string empty_checksum;
    store->MergeUpdate(
        HashPrefixMapView(prefix_map_old.begin(), prefix_map_old.end()),
        HashPrefixMapView(prefix_map_additions.begin(),
                          prefix_map_additions.end()),
        &raw_removals, empty_checksum);
#ifndef NDEBUG
    DisplayHashPrefixMapDetails(store->hash_prefix_map_->view());
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
  static void PopulateHashPrefixMap(
      base::span<const uint8_t>* data,
      std::unordered_map<PrefixSize, HashPrefixes>* hash_prefix_map) {
    uint8_t datum;
    if (!GetDatum(data, &datum)) {
      return;
    }

    // Prefix size is defined to be between |kMinHashPrefixLength| and
    // |kMaxHashPrefixLength| but we are going to limit them to smaller sizes so
    // that we have a higher chance of actually populating the
    // |hash_prefix_map| for smaller inputs.
    PrefixSize prefix_size = kMinHashPrefixLengthForFuzzing +
                             (datum % (kMaxHashPrefixLengthForFuzzing -
                                       kMinHashPrefixLengthForFuzzing + 1));

    if (!GetDatum(data, &datum)) {
      return;
    }
    size_t prefixes_list_size = datum;
    // This |prefixes_list_size| is the length of the list of prefixes to be
    // added. It can't be larger than the remaining buffer.
    if (data->size() < prefixes_list_size) {
      prefixes_list_size = data->size();
    }
    std::string prefixes(data->begin(), data->begin() + prefixes_list_size);
    *data = data->subspan(prefixes_list_size);
    V4Store::AddUnlumpedHashes(prefix_size, prefixes, hash_prefix_map);
#ifndef NDEBUG
    DisplayHashPrefixMapDetails(
        HashPrefixMapView(hash_prefix_map->begin(), hash_prefix_map->end()));
#endif
  }

  static bool GetDatum(base::span<const uint8_t>* data, uint8_t* datum) {
    if (data->size() == 0) {
      return false;
    }
    *datum = (*data)[0];
    *data = data->subspan(1);
    return true;
  }

  static void DisplayHashPrefixMapDetails(
      const HashPrefixMapView& hash_prefix_map) {
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
  // SAFETY: libfuzzer guarantees a valid pointer and size pair.
  return safe_browsing::V4StoreFuzzer::FuzzMergeUpdate(
      UNSAFE_BUFFERS(base::span(data, size)));
}
