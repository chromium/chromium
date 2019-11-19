// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/indexed_ruleset.h"

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/strings/string_piece.h"
#include "components/subresource_filter/core/common/first_party_origin.h"
#include "components/subresource_filter/core/common/unindexed_ruleset.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "url/gurl.h"
#include "url/origin.h"

struct TestCase {
  TestCase() { CHECK(base::i18n::InitializeICU()); }
  base::AtExitManager at_exit_manager;
};

TestCase* test_case = new TestCase();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzed_data(data, size);

  // Split the input into two sections, the URL to check, and the ruleset
  // itself.
  std::string url_string = fuzzed_data.ConsumeRandomLengthString(1024);
  const GURL url_to_check = GURL(url_string);

  // Error out early if the URL isn't valid, since we early-return in the
  // IndexedRulesetMatcher pretty early if this is the case.
  if (!url_to_check.is_valid())
    return 0;

  std::vector<uint8_t> remaining_bytes =
      fuzzed_data.ConsumeRemainingBytes<uint8_t>();

  // First, interpret the remaining fuzzed data as an unindexed ruleset.
  google::protobuf::io::ArrayInputStream input_stream(remaining_bytes.data(),
                                                      remaining_bytes.size());
  subresource_filter::UnindexedRulesetReader reader(&input_stream);

  // Use the unindexed ruleset to build a flat indexed ruleset.
  subresource_filter::RulesetIndexer indexer;
  url_pattern_index::proto::FilteringRules ruleset_chunk;
  while (reader.ReadNextChunk(&ruleset_chunk)) {
    for (const auto& rule : ruleset_chunk.url_rules())
      indexer.AddUrlRule(rule);
  }
  indexer.Finish();

  // Error out if we were unable to fully read the unindexed version.
  if (reader.num_bytes_read() != static_cast<int64_t>(remaining_bytes.size()))
    return 0;

  CHECK(subresource_filter::IndexedRulesetMatcher::Verify(
      indexer.data(), indexer.size(), indexer.GetChecksum()));

  // Lastly, read into the indexed ruleset by matching the URL from the
  // beginning of the fuzzed data.
  subresource_filter::IndexedRulesetMatcher matcher(indexer.data(),
                                                    indexer.size());
  // TODO(csharrison): Consider fuzzing things like the parent origin, the
  // activation type, and the element type.
  matcher.ShouldDisableFilteringForDocument(
      url_to_check, url::Origin(),
      url_pattern_index::proto::ACTIVATION_TYPE_DOCUMENT);
  matcher.ShouldDisallowResourceLoad(
      url_to_check, subresource_filter::FirstPartyOrigin(url::Origin()),
      url_pattern_index::proto::ELEMENT_TYPE_SCRIPT,
      false /* disable_generic_rules */);
  return 0;
}
