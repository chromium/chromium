// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/common/spellcheck_result.h"
#include <vector>

SpellCheckResult::SpellCheckResult(Decoration d,
                                   int loc,
                                   int len,
                                   const std::vector<std::u16string>& rep,
                                   bool should_hide_suggestion_menu)
    : decoration(d),
      location(loc),
      length(len),
      replacements(rep),
      should_hide_suggestion_menu(should_hide_suggestion_menu) {}

SpellCheckResult::SpellCheckResult(Decoration d,
                                   int loc,
                                   int len,
                                   const std::u16string& rep,
                                   bool should_hide_suggestion_menu)
    : decoration(d),
      location(loc),
      length(len),
      replacements(std::vector<std::u16string>({rep})),
      should_hide_suggestion_menu(should_hide_suggestion_menu) {}

SpellCheckResult::~SpellCheckResult() = default;

SpellCheckResult::SpellCheckResult(const SpellCheckResult&) = default;
