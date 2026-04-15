// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_PROMO_TRACKER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_PROMO_TRACKER_H_

#include <optional>

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/session_id.h"

namespace autofill {

// Detects a cross-tab copy-paste sequence to trigger the "At-Memory" promo
// bubble.
class AtMemoryPromoTracker : public KeyedService {
 public:
  AtMemoryPromoTracker();
  AtMemoryPromoTracker(const AtMemoryPromoTracker&) = delete;
  AtMemoryPromoTracker& operator=(const AtMemoryPromoTracker&) = delete;
  ~AtMemoryPromoTracker() override;

  // KeyedService:
  void Shutdown() override;

  // Records a copy action.
  void OnCopy(SessionID tab_id);

  // Records a paste action and returns true if it completes a valid sequence
  // (Paste in a different tab than the copy, within the time limit).
  bool OnPaste(SessionID tab_id);

 private:
  struct ActionRecord {
    SessionID tab_id;
    base::TimeTicks time;
  };

  std::optional<ActionRecord> last_copy_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_PROMO_TRACKER_H_
