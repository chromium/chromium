// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_OBSERVER_H_
#define COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_OBSERVER_H_

#include "components/sessions/core/sessions_export.h"

namespace sessions {

class TabRestoreService;

// Observer is notified when the set of entries managed by TabRestoreService
// changes in some way.
class SESSIONS_EXPORT TabRestoreServiceObserver {
 public:
  // Sent when the set of entries changes in some way.
  virtual void TabRestoreServiceChanged(TabRestoreService* service) {}

  // Sent to all remaining Observers when TabRestoreService's
  // destructor is run.
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service) = 0;

  // Sent when TabRestoreService finishes loading.
  virtual void TabRestoreServiceLoaded(TabRestoreService* service) {}

 protected:
  virtual ~TabRestoreServiceObserver() = default;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_OBSERVER_H_
