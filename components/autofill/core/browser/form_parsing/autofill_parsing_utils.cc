// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"

namespace autofill {

MatchingPattern::MatchingPattern() = default;
MatchingPattern::MatchingPattern(const MatchingPattern& mp) = default;
MatchingPattern& MatchingPattern::operator=(const MatchingPattern& mp) =
    default;

MatchingPattern::~MatchingPattern() = default;

}  // namespace autofill
