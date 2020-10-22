// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_PROVIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_PROVIDER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// Base class for the Pattern Provider. This class contains the implementation
// for providing the matching patterns. Different subclasses provide different
// ways to load the data in for further use.
class PatternProvider {
 public:
  // Shorthand for the map structure used to store patterns.
  using Map = std::map<std::string,
                       std::map<std::string, std::vector<MatchingPattern>>>;

  // Returns a reference to the global Pattern Provider.
  static PatternProvider& GetInstance();

  // Setter for loading patterns from external storage.
  void SetPatterns(const Map patterns,
                   const base::Version version,
                   const bool overwrite_equal_version);

  // Provides us with all patterns that can match our field type and page
  // language.
  const std::vector<MatchingPattern> GetMatchPatterns(
      ServerFieldType type,
      const std::string& page_language);

  const std::vector<MatchingPattern> GetMatchPatterns(
      const std::string& pattern_name,
      const std::string& page_language);

  // Provides us with all patterns that can match our field type.
  const std::vector<MatchingPattern> GetAllPatternsBaseOnType(
      ServerFieldType type);

  const std::vector<MatchingPattern> GetAllPatternsBaseOnType(
      const std::string& type);
  // desc
  void EnrichPatternsWithEnVersion();

 protected:
  PatternProvider();
  ~PatternProvider();

  // Local map to store a vector of patterns keyed by field type and
  // page language.
  Map patterns_;

  // Version for keeping track which pattern set is in use.
  base::Version pattern_version_;

  // Sets a provider to be used for tests.
  static void SetPatternProviderForTesting(PatternProvider* pattern_provider);

  // Resets the provider pointer if the object behind it gets deleted.
  static void ResetPatternProvider();

 private:
  // Func to sort the incoming map by score.
  void SortPatternsByScore(std::vector<MatchingPattern>& patterns);

  // Sequence checker to ensure thread-safety for pattern swapping.
  // All functions accessing the |patterns_| member variable are
  // expected to be called from the UI thread.
  SEQUENCE_CHECKER(sequence_checker_);

  FRIEND_TEST_ALL_PREFIXES(AutofillPatternProviderPipelineTest,
                           TestParsingEquivalent);
  FRIEND_TEST_ALL_PREFIXES(AutofillPatternProviderPipelineTest,
                           DefaultPatternProviderLoads);

  friend class base::NoDestructor<PatternProvider>;
};
}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_PROVIDER_H_
