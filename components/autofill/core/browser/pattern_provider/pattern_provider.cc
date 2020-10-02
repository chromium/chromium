// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/pattern_provider/pattern_provider.h"

#include <algorithm>
#include <iostream>
#include <string>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/pattern_provider/pattern_configuration_parser.h"

namespace autofill {
namespace {
PatternProvider* g_pattern_provider = nullptr;
}

PatternProvider::PatternProvider() = default;
PatternProvider::~PatternProvider() = default;

void PatternProvider::SetPatterns(PatternProvider::Map patterns,
                                  const base::Version version,
                                  const bool overwrite_equal_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!pattern_version_.IsValid() || pattern_version_ < version ||
      (overwrite_equal_version && pattern_version_ == version)) {
    patterns_ = patterns;
    pattern_version_ = version;
  }
}

const std::vector<MatchingPattern>& PatternProvider::GetMatchPatterns(
    const std::string& pattern_name,
    const std::string& page_language) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return patterns_[pattern_name][page_language];
}

const std::vector<MatchingPattern>& PatternProvider::GetMatchPatterns(
    ServerFieldType type,
    const std::string& page_language) {
  std::string pattern_name = AutofillType(type).ToString();
  return GetMatchPatterns(pattern_name, page_language);
}

// static.
PatternProvider& PatternProvider::GetInstance() {
  if (!g_pattern_provider) {
    static base::NoDestructor<PatternProvider> instance;
    g_pattern_provider = instance.get();
    field_type_parsing::PopulateFromResourceBundle();
  }
  return *g_pattern_provider;
}

// static.
void PatternProvider::SetPatternProviderForTesting(
    PatternProvider* pattern_provider) {
  DCHECK(pattern_provider);
  g_pattern_provider = pattern_provider;
}

// static.
void PatternProvider::ResetPatternProvider() {
  g_pattern_provider = nullptr;
}

}  //  namespace autofill
