// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_CREATOR_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_CREATOR_H_

#include <stdint.h>

#include <string_view>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "components/url_pattern_index/proto/rules.pb.h"

namespace subresource_filter {
namespace testing {

// Encapsulates a testing subresource filtering ruleset serialized either in
// indexed or unindexed format. The ruleset |contents| can be accessed directly
// as a byte buffer, as well as through the file |path| pointing to a temporary
// file that is cleaned up when the TestRulesetCreator is destroyed.
struct TestRuleset {
  TestRuleset();
  ~TestRuleset();

  // Convenience function to open a read-only file handle to |ruleset|.
  static base::File Open(const TestRuleset& ruleset);

  // Corrupts the |ruleset| file by truncating its tail of a certain size.
  static void CorruptByTruncating(const TestRuleset& ruleset, size_t tail_size);

  // Overrides all bytes in the [from..to) range by |fill_with|.
  static void CorruptByFilling(const TestRuleset& ruleset,
                               size_t from,
                               size_t to,
                               uint8_t fill_with);

  // TODO(ericrobinson): Add a checksum field here that is handled like it
  // is for checksums in the real rulesets.
  std::vector<uint8_t> contents;
  base::FilePath path;
};

// Encapsulates the same ruleset in both indexed and unindexed formats.
struct TestRulesetPair {
  TestRulesetPair();
  ~TestRulesetPair();

  TestRuleset unindexed;
  TestRuleset indexed;
};

// Helper class to create subresource filtering rulesets for testing.
//
// All temporary files and paths are cleaned up when the instance goes out of
// scope, but file handles already open can still be used and read even after
// this has happened.
class TestRulesetCreator {
 public:
  TestRulesetCreator();

  TestRulesetCreator(const TestRulesetCreator&) = delete;
  TestRulesetCreator& operator=(const TestRulesetCreator&) = delete;

  ~TestRulesetCreator();

  // Creates both the indexed and unindexed versions of a testing ruleset that
  // consists of single filtering rule that disallows subresource loads from URL
  // paths having the given |suffix|.
  // Enclose call in ASSERT_NO_FATAL_FAILURE to detect errors.
  void CreateRulesetToDisallowURLsWithPathSuffix(
      std::string_view suffix,
      TestRulesetPair* test_ruleset_pair);

  // Same as above, but only creates an unindexed ruleset.
  void CreateUnindexedRulesetToDisallowURLsWithPathSuffix(
      std::string_view suffix,
      TestRuleset* test_unindexed_ruleset);

  // Creates both the indexed and unindexed versions of a testing ruleset that
  // consists of filtering rules that disallow subresource loads from URLs
  // containing any of the given `substrings`. Enclose call in
  // ASSERT_NO_FATAL_FAILURE to detect errors.
  void CreateRulesetToDisallowURLWithSubstrings(
      std::vector<std::string_view> substrings,
      TestRulesetPair* test_ruleset_pair);

  // Similar to CreateRulesetToDisallowURLsWithPathSuffix, but the resulting
  // ruleset consists of |num_of_suffixes| rules, each of them disallowing URLs
  // with suffixes of the form |suffix|_k, 0 <= k < |num_of_suffixes|.
  void CreateRulesetToDisallowURLsWithManySuffixes(
      std::string_view suffix,
      int num_of_suffixes,
      TestRulesetPair* test_ruleset_pair);

  void CreateRulesetWithRules(
      const std::vector<url_pattern_index::proto::UrlRule>& rules,
      TestRulesetPair* test_ruleset_pair);
  void CreateUnindexedRulesetWithRules(
      const std::vector<url_pattern_index::proto::UrlRule>& rules,
      TestRuleset* test_unindexed_ruleset);

  // Returns a unique |path| that is valid for the lifetime of this instance.
  // No file at |path| will be automatically created.
  void GetUniqueTemporaryPath(base::FilePath* path);

 private:
  // Writes the |ruleset_contents| to a temporary file, and initializes
  // |ruleset| to have the same |contents|, and the |path| to this file.
  void CreateTestRulesetFromContents(std::vector<uint8_t> ruleset_contents,
                                     TestRuleset* ruleset);

  std::unique_ptr<base::ScopedTempDir> scoped_temp_dir_;
  int next_unique_file_suffix = 1;
};

}  // namespace testing
}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_CREATOR_H_
