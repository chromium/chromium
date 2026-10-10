// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_TEST_UTILS_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_TEST_UTILS_H_

#include <iosfwd>

#include "components/history/core/browser/journeys/journey_row.h"

namespace history::journeys {

// gtest printers for readable failure messages. They live in this shared
// test-only file so that every test printing these types uses the same printer.
void PrintTo(const JourneyHistoryEntry& entry, std::ostream* os);
void PrintTo(const JourneyHistoryEntryCollection& collection, std::ostream* os);

}  // namespace history::journeys

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_TEST_UTILS_H_
