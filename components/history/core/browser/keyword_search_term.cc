// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/keyword_search_term.h"

#include <cmath>

namespace history {

KeywordSearchTermVisit::~KeywordSearchTermVisit() = default;

double KeywordSearchTermVisit::GetFrecency(base::Time now,
                                           int recency_decay_unit_sec,
                                           double frequency_exponent) const {
  const double recency_sec = base::TimeDelta(now - last_visit_time).InSeconds();
  const double recency_decayed =
      recency_decay_unit_sec / (recency_sec + recency_decay_unit_sec);
  const double frequency_powered = pow(visit_count, frequency_exponent);
  return frequency_powered * recency_decayed;
}

KeywordSearchTermRow::KeywordSearchTermRow() : keyword_id(0), url_id(0) {}

KeywordSearchTermRow::KeywordSearchTermRow(const KeywordSearchTermRow& other) =
    default;

KeywordSearchTermRow::~KeywordSearchTermRow() {}

}  // namespace history
