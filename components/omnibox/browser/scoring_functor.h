// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SCORING_FUNCTOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_SCORING_FUNCTOR_H_

#include <stddef.h>

#include "components/query_parser/snippet.h"

// `for_each()` helper functor to score how similar an input and suggestion text
// are. Can be used for titles or URLs. For each string match, computes the
// product of:
// 1) how many characters matched; this will range (0, `length_`].
// 2) where the match is positioned (0, 1].
// These products are summed; this too will range (0, `length_`].
class ScoringFunctor {
 public:
  // `length` is the length of the suggestion text.
  explicit ScoringFunctor(size_t length);

  // Called for each match.
  void operator()(const query_parser::Snippet::MatchPosition& match);

  // Returns the accumulated score.
  double ScoringFactor();

 private:
  double length_;
  double scoring_factor_ = 0;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_SCORING_FUNCTOR_H_
