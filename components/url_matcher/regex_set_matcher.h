// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_MATCHER_REGEX_SET_MATCHER_H_
#define COMPONENTS_URL_MATCHER_REGEX_SET_MATCHER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "components/url_matcher/string_pattern.h"
#include "components/url_matcher/substring_set_matcher.h"
#include "components/url_matcher/url_matcher_export.h"

namespace re2 {
class FilteredRE2;
}

namespace url_matcher {

// Efficiently matches URLs against a collection of regular expressions,
// using FilteredRE2 to reduce the number of regexes that must be matched
// by pre-filtering with substring matching. See:
// http://swtch.com/~rsc/regexp/regexp3.html#analysis
class URL_MATCHER_EXPORT RegexSetMatcher {
 public:
  RegexSetMatcher();
  virtual ~RegexSetMatcher();

  // Adds the regex patterns in |regex_list| to the matcher. Also rebuilds
  // the FilteredRE2 matcher; thus, for efficiency, prefer adding multiple
  // patterns at once.
  // Ownership of the patterns remains with the caller.
  void AddPatterns(const std::vector<const StringPattern*>& regex_list);

  // Removes all regex patterns.
  void ClearPatterns();

  // Appends the IDs of regular expressions in our set that match the |text|
  // to |matches|.
  bool Match(const std::string& text,
             std::set<StringPattern::ID>* matches) const;

  bool IsEmpty() const;

 private:
  typedef int RE2ID;
  typedef std::map<StringPattern::ID, const StringPattern*> RegexMap;
  typedef std::vector<StringPattern::ID> RE2IDMap;

  // Use Aho-Corasick SubstringSetMatcher to find which literal patterns
  // match the |text|.
  std::vector<RE2ID> FindSubstringMatches(const std::string& text) const;

  // Rebuild FilteredRE2 from scratch. Needs to be called whenever
  // our set of regexes changes.
  // TODO(yoz): investigate if it could be done incrementally;
  // apparently not supported by FilteredRE2.
  void RebuildMatcher();

  // Mapping of regex StringPattern::IDs to regexes.
  RegexMap regexes_;
  // Mapping of RE2IDs from FilteredRE2 (which are assigned in order)
  // to regex StringPattern::IDs.
  RE2IDMap re2_id_map_;

  std::unique_ptr<re2::FilteredRE2> filtered_re2_;
  std::unique_ptr<SubstringSetMatcher> substring_matcher_;
};

}  // namespace url_matcher

#endif  // COMPONENTS_URL_MATCHER_REGEX_SET_MATCHER_H_
