// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_test_utils.h"

#include <ostream>

#include "base/time/time.h"

namespace history::journeys {

void PrintTo(const JourneyHistoryEntry& entry, std::ostream* os) {
  *os << "JourneyHistoryEntry("
      << entry.visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds() << ")";
}

void PrintTo(const JourneyHistoryEntryCollection& collection,
             std::ostream* os) {
  *os << "JourneyHistoryEntryCollection(\"" << collection.title << "\", {";
  for (size_t i = 0; i < collection.items.size(); ++i) {
    if (i > 0) {
      *os << ", ";
    }
    PrintTo(collection.items[i], os);
  }
  *os << "})";
}

}  // namespace history::journeys
