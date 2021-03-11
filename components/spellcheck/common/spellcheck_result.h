// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_RESULT_H_
#define COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_RESULT_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/strings/string16.h"

// This class mirrors blink::WebTextCheckingResult which holds a
// misspelled range inside the checked text. It also contains a
// possible replacement of the misspelling if it is available.
struct SpellCheckResult {
  enum Decoration {
    // Red underline for misspelled words.
    SPELLING,

    // Gray underline for correctly spelled words that are incorrectly used in
    // their context.
    GRAMMAR,
    LAST = GRAMMAR,
  };

  explicit SpellCheckResult(
      Decoration d = SPELLING,
      int loc = 0,
      int len = 0,
      const std::vector<base::string16>& rep = std::vector<base::string16>());

  explicit SpellCheckResult(Decoration d,
                            int loc,
                            int len,
                            const base::string16& rep);

  ~SpellCheckResult();
  SpellCheckResult(const SpellCheckResult&);

  Decoration decoration;
  int location;
  int length;
  std::vector<base::string16> replacements;
  bool spelling_service_used = false;
};

#endif  // COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_RESULT_H_
