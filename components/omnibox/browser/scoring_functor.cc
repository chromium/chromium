// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scoring_functor.h"

#include <stddef.h>

#include <algorithm>

#include "base/check.h"
#include "components/query_parser/snippet.h"

ScoringFunctor::ScoringFunctor(size_t length)
    : length_(static_cast<double>(length)) {}

void ScoringFunctor::operator()(
    const query_parser::Snippet::MatchPosition& match) {
  DCHECK(match.second <= length_);
  double match_length = static_cast<double>(match.second - match.first);
  scoring_factor_ += match_length * (length_ - match.first) / length_;
}

double ScoringFunctor::ScoringFactor() {
  return scoring_factor_;
}
