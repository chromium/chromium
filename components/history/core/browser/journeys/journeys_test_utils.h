// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_TEST_UTILS_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_TEST_UTILS_H_

#include <cstdint>
#include <iosfwd>

#include "base/time/time.h"
#include "components/history/core/browser/journeys/journey.h"
#include "components/history/core/browser/journeys/journey_row.h"

namespace history::journeys {

// Returns the time `micros` microseconds after the Windows epoch, the
// representation used for timestamps in the journeys tables.
base::Time TimeFromMicros(int64_t micros);

// gtest printers for readable failure messages. They live in this shared
// test-only file so that every test printing these types uses the same printer.
void PrintTo(const JourneyHistoryEntry& entry, std::ostream* os);
void PrintTo(const JourneyHistoryEntryCollection& collection, std::ostream* os);
void PrintTo(const JourneyVisit& visit, std::ostream* os);
void PrintTo(const JourneyVisitCollection& collection, std::ostream* os);

}  // namespace history::journeys

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_TEST_UTILS_H_
