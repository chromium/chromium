// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/common/spellcheck_result.h"
#include <vector>

SpellCheckResult::SpellCheckResult(Decoration d,
                                   int loc,
                                   int len,
                                   const std::vector<std::u16string>& rep)
    : decoration(d), location(loc), length(len), replacements(rep) {}

SpellCheckResult::SpellCheckResult(Decoration d,
                                   int loc,
                                   int len,
                                   const std::u16string& rep)
    : decoration(d),
      location(loc),
      length(len),
      replacements(std::vector<std::u16string>({rep})) {}

SpellCheckResult::~SpellCheckResult() = default;

SpellCheckResult::SpellCheckResult(const SpellCheckResult&) = default;
