// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EVENT_GLOBAL_TRACKER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EVENT_GLOBAL_TRACKER_H_

#include "base/callback_list.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

struct OmniboxLog;

// Omnibox code tracks events on a per-user-context basis, but there are several
// clients who need to observe these events for all user contexts (e.g., all
// Profiles in the //chrome embedder). This class serves as an intermediary to
// bridge the gap: omnibox code calls the OmniboxEventGlobalTracker singleton on
// an event of interest, and it then forwards the event to its registered
// observers.
class OmniboxEventGlobalTracker {
 public:
  using OnURLOpenedCallbackList =
      base::RepeatingCallbackList<void(OmniboxLog*)>;
  using OnURLOpenedCallback = OnURLOpenedCallbackList::CallbackType;

  // Returns the instance of OmniboxEventGlobalTracker.
  static OmniboxEventGlobalTracker* GetInstance();

  // Registers `cb` to be invoked when user open a URL from the omnibox.
  base::CallbackListSubscription RegisterCallback(
      const OnURLOpenedCallback& cb);

  // Called to notify all registered callbacks that a URL was opened from the
  // omnibox.
  void OnURLOpened(OmniboxLog* log);

 private:
  friend struct base::DefaultSingletonTraits<OmniboxEventGlobalTracker>;

  OmniboxEventGlobalTracker();
  ~OmniboxEventGlobalTracker();
  OmniboxEventGlobalTracker(const OmniboxEventGlobalTracker&) = delete;
  OmniboxEventGlobalTracker& operator=(const OmniboxEventGlobalTracker&) =
      delete;

  OnURLOpenedCallbackList on_url_opened_callback_list_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EVENT_GLOBAL_TRACKER_H_
