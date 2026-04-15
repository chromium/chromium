// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory_promo_tracker.h"

namespace autofill {

namespace {

// Maximum duration between copy and paste in a valid sequence.
constexpr base::TimeDelta kSequenceDuration = base::Seconds(60);

}  // namespace

AtMemoryPromoTracker::AtMemoryPromoTracker() = default;

AtMemoryPromoTracker::~AtMemoryPromoTracker() = default;

void AtMemoryPromoTracker::Shutdown() {
  last_copy_.reset();
}

void AtMemoryPromoTracker::OnCopy(SessionID tab_id) {
  last_copy_.emplace(tab_id, base::TimeTicks::Now());
}

bool AtMemoryPromoTracker::OnPaste(SessionID tab_id) {
  if (!last_copy_.has_value()) {
    return false;
  }

  bool is_different_tab = last_copy_->tab_id != tab_id;
  bool is_recent =
      (base::TimeTicks::Now() - last_copy_->time) <= kSequenceDuration;

  if (is_different_tab && is_recent) {
    last_copy_.reset();
    return true;
  }

  return false;
}

}  // namespace autofill
