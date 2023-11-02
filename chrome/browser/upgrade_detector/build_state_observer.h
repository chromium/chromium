// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_BUILD_STATE_OBSERVER_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_BUILD_STATE_OBSERVER_H_

#include "base/observer_list_types.h"

class BuildState;

// An observer of changes to the browser's BuildState.
class BuildStateObserver : public base::CheckedObserver {
 public:
  // Called when the installed version of the browser changes. For example, when
  // an update is installed on the machine and a restart is needed for it to
  // become the running version. |build_state| is the browser's state.
  virtual void OnUpdate(const BuildState* build_state) = 0;
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_BUILD_STATE_OBSERVER_H_
