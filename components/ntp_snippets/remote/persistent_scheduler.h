// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_PERSISTENT_SCHEDULER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_PERSISTENT_SCHEDULER_H_

#include "base/time/time.h"

namespace ntp_snippets {

// Interface to schedule persistent periodic fetches for remote suggestions, OS-
// dependent. These persistent fetches must get triggered according to their
// schedule independent of whether Chrome is running at that moment.
//
// Once per period, the concrete implementation should call
// RemoteSuggestionsScheduler::OnPersistentSchedulerWakeUp() where the scheduler
// object is obtained from ContentSuggestionsService.
class PersistentScheduler {
 public:
  PersistentScheduler(const PersistentScheduler&) = delete;
  PersistentScheduler& operator=(const PersistentScheduler&) = delete;
  // Schedule periodic fetching of remote suggestions, with different periods
  // depending on network state. Any of the periods can be zero to indicate that
  // the corresponding task should not be scheduled. Returns whether the
  // scheduling was successful.
  virtual bool Schedule(base::TimeDelta period_wifi,
                        base::TimeDelta period_fallback) = 0;

  // Cancel any scheduled tasks. Equivalent to Schedule(0, 0). Returns whether
  // the scheduling was successful.
  virtual bool Unschedule() = 0;

  // TODO(jkrcal): Get this information exposed in the platform-independent
  // net::NetworkChangeNotifier and remove this function.
  virtual bool IsOnUnmeteredConnection() = 0;

 protected:
  PersistentScheduler() = default;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_PERSISTENT_SCHEDULER_H_
