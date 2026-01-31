// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_RESULT_H_
#define COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_RESULT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "components/spellcheck/common/spellcheck_decoration.h"

// This class mirrors blink::WebTextCheckingResult which holds a
// misspelled range inside the checked text. It also contains a
// possible replacement of the misspelling if it is available.
struct SpellCheckResult {
  explicit SpellCheckResult(
      spellcheck::Decoration d = spellcheck::Decoration::SPELLING,
      int loc = 0,
      int len = 0,
      const std::vector<std::u16string>& rep = std::vector<std::u16string>(),
      bool should_hide_suggestion_menu = false);

  explicit SpellCheckResult(spellcheck::Decoration d,
                            int loc,
                            int len,
                            const std::u16string& rep,
                            bool should_hide_suggestion_menu = false);

  ~SpellCheckResult();
  SpellCheckResult(const SpellCheckResult&);

  spellcheck::Decoration decoration;

  // The zero-based index where the misspelling starts. For spell check results
  // returned by the local spell check infrastructure, this is measured by
  // the code point count, i.e. each surrogate pair, such as emojis, will count
  // for 2 positions. For results returned by enhanced (server side) spell
  // check, positions are based on "logical" characters, i.e. emojis and their
  // modifiers count for 1.
  int location;

  // The length of the misspelled word. The same code point / logical character
  // count distinction as for `location` applies.
  int length;

  std::vector<std::u16string> replacements;
  bool spelling_service_used = false;

  // Whether to hide the suggestion menu that is rendered by chrome.
  bool should_hide_suggestion_menu = false;
};

#endif  // COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_RESULT_H_
