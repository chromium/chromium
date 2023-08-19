// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SCHEDULER_UI_INTERFACE_H_
#define CHROME_BROWSER_VR_SCHEDULER_UI_INTERFACE_H_

#include "chrome/browser/vr/vr_base_export.h"

namespace vr {

class VR_BASE_EXPORT SchedulerUiInterface {
 public:
  virtual ~SchedulerUiInterface() {}

  virtual void OnWebXrFrameAvailable() = 0;
  virtual void OnWebXrTimedOut() = 0;
  virtual void OnWebXrTimeoutImminent() = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SCHEDULER_UI_INTERFACE_H_
