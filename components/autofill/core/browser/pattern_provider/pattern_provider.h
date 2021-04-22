// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_PROVIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

// Base class for the Pattern Provider. This class contains the implementation
// for providing the matching patterns. Different subclasses provide different
// ways to load the data in for further use.
class PatternProvider {
 public:
  // The outer keys are field types or other pattern names. The inner keys are
  // page languages in lower case.
  using Map = std::map<std::string,
                       std::map<LanguageCode, std::vector<MatchingPattern>>>;

  // Returns a reference to the global Pattern Provider.
  static PatternProvider& GetInstance();

  // Setter for loading patterns from external storage.
  void SetPatterns(const Map patterns, const base::Version& version);

  // Find the patterns for a given ServerFieldType and for a given
  // |page_language|.
  const std::vector<MatchingPattern> GetMatchPatterns(
      ServerFieldType type,
      const LanguageCode& page_language) const;

  // Find the patterns for a given |pattern_name| and a given |page_language|.
  const std::vector<MatchingPattern> GetMatchPatterns(
      const std::string& pattern_name,
      const LanguageCode& page_language) const;

  // Find all patterns, across all languages, for a given server field |type|.
  const std::vector<MatchingPattern> GetAllPatternsByType(
      ServerFieldType type) const;

  // Find all patterns, across all languages, for a given server field |type|.
  const std::vector<MatchingPattern> GetAllPatternsByType(
      const std::string& type) const;

 protected:
  PatternProvider();
  ~PatternProvider();

 private:
  FRIEND_TEST_ALL_PREFIXES(AutofillPatternProviderTest, TestDefaultEqualsJson);

  friend class base::NoDestructor<PatternProvider>;

  // Sequence checker to ensure thread-safety for pattern swapping.
  // All functions accessing the |patterns_| member variable are
  // expected to be called from the UI thread.
  SEQUENCE_CHECKER(sequence_checker_);

  // Local map to store a vector of patterns keyed by field type and
  // page language.
  Map patterns_;

  // Version for keeping track which pattern set is currently used.
  base::Version pattern_version_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_PROVIDER_H_
