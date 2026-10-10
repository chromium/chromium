// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_test_utils.h"

#include <ostream>

#include "base/strings/utf_string_conversions.h"
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

void PrintTo(const JourneyVisit& visit, std::ostream* os) {
  *os << "JourneyVisit(" << visit.url.possibly_invalid_spec() << ", \""
      << base::UTF16ToUTF8(visit.title) << "\", "
      << visit.visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds() << ", "
      << (visit.is_foreign ? "foreign" : "local") << ")";
}

void PrintTo(const JourneyVisitCollection& collection, std::ostream* os) {
  *os << "JourneyVisitCollection(\"" << collection.title << "\", {";
  for (size_t i = 0; i < collection.visits.size(); ++i) {
    if (i > 0) {
      *os << ", ";
    }
    PrintTo(collection.visits[i], os);
  }
  *os << "})";
}

}  // namespace history::journeys
