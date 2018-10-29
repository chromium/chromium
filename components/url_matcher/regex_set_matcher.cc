// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/regex_set_matcher.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/url_matcher/substring_set_matcher.h"
#include "third_party/re2/src/re2/filtered_re2.h"
#include "third_party/re2/src/re2/re2.h"

namespace url_matcher {

RegexSetMatcher::RegexSetMatcher() {}

RegexSetMatcher::~RegexSetMatcher() {
  DeleteSubstringPatterns();
}

void RegexSetMatcher::AddPatterns(
    const std::vector<const StringPattern*>& regex_list) {
  if (regex_list.empty())
    return;
  for (size_t i = 0; i < regex_list.size(); ++i) {
    regexes_[regex_list[i]->id()] = regex_list[i];
  }

  RebuildMatcher();
}

void RegexSetMatcher::ClearPatterns() {
  regexes_.clear();
  RebuildMatcher();
}

bool RegexSetMatcher::Match(const std::string& text,
                            std::set<StringPattern::ID>* matches) const {
  size_t old_number_of_matches = matches->size();
  if (regexes_.empty())
    return false;
  if (!filtered_re2_) {
    LOG(ERROR) << "RegexSetMatcher was not initialized";
    return false;
  }

  // FilteredRE2 expects lowercase for prefiltering, but we still
  // match case-sensitively.
  std::vector<RE2ID> atoms(FindSubstringMatches(base::ToLowerASCII(text)));

  std::vector<RE2ID> re2_ids;
  filtered_re2_->AllMatches(text, atoms, &re2_ids);

  for (size_t i = 0; i < re2_ids.size(); ++i) {
    StringPattern::ID id = re2_id_map_[re2_ids[i]];
    matches->insert(id);
  }
  return old_number_of_matches != matches->size();
}

bool RegexSetMatcher::IsEmpty() const {
  return regexes_.empty();
}

std::vector<RegexSetMatcher::RE2ID> RegexSetMatcher::FindSubstringMatches(
    const std::string& text) const {
  std::set<int> atoms_set;
  substring_matcher_->Match(text, &atoms_set);
  return std::vector<RE2ID>(atoms_set.begin(), atoms_set.end());
}

void RegexSetMatcher::RebuildMatcher() {
  re2_id_map_.clear();
  filtered_re2_.reset(new re2::FilteredRE2());
  if (regexes_.empty())
    return;

  for (auto it = regexes_.begin(); it != regexes_.end(); ++it) {
    RE2ID re2_id;
    RE2::ErrorCode error = filtered_re2_->Add(
        it->second->pattern(), RE2::DefaultOptions, &re2_id);
    if (error == RE2::NoError) {
      DCHECK_EQ(static_cast<RE2ID>(re2_id_map_.size()), re2_id);
      re2_id_map_.push_back(it->first);
    } else {
      // Unparseable regexes should have been rejected already in
      // URLMatcherFactory::CreateURLMatchesCondition.
      LOG(ERROR) << "Could not parse regex (id=" << it->first << ", "
                 << it->second->pattern() << ")";
    }
  }

  std::vector<std::string> strings_to_match;
  filtered_re2_->Compile(&strings_to_match);

  substring_matcher_.reset(new SubstringSetMatcher);
  DeleteSubstringPatterns();
  // Build SubstringSetMatcher from |strings_to_match|.
  // SubstringSetMatcher doesn't own its strings.
  for (size_t i = 0; i < strings_to_match.size(); ++i) {
    substring_patterns_.push_back(
        std::make_unique<StringPattern>(strings_to_match[i], i));
  }
  std::vector<const StringPattern*> patterns;
  for (const auto& pattern : substring_patterns_)
    patterns.push_back(pattern.get());
  substring_matcher_->RegisterPatterns(patterns);
}

void RegexSetMatcher::DeleteSubstringPatterns() {
  substring_patterns_.clear();
}

}  // namespace url_matcher
