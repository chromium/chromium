// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TEST_EVENT_UTIL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TEST_EVENT_UTIL_H_

#include <stdint.h>

namespace feature_engagement {

class Event;

namespace test {

// Adds an Event count entry for |event|.
void SetEventCountForDay(Event* event, uint32_t day, uint32_t count);

// Adds a snooze count entry for |event|.
void SetSnoozeCountForDay(Event* event, uint32_t day, uint32_t count);

// Verifies that the given |event| contains a |day| with the correct |count|,
// and that the day only exists a single time.
void VerifyEventCount(const Event* event, uint32_t day, uint32_t count);

// Verifies that the event |a| and |b| contain the exact same data.
void VerifyEventsEqual(const Event* a, const Event* b);

}  // namespace test
}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TEST_EVENT_UTIL_H_
