// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_SYNC_OBSERVER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_SYNC_OBSERVER_H_

#include "base/functional/callback.h"

namespace safe_browsing {

// Interface used to observe sync events.
class SafeBrowsingSyncObserver {
 public:
  using Callback = base::RepeatingCallback<void()>;

  virtual ~SafeBrowsingSyncObserver() = default;

  // Starts to observe history sync state changed events. `callback` will be
  // invoked if the history sync state has changed. `callback` can be invoked
  // multiple times.
  virtual void ObserveHistorySyncStateChanged(Callback callback) = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_SYNC_OBSERVER_H_
