// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TRANSITION_H_
#define CHROME_BROWSER_VR_TRANSITION_H_

#include <set>

#include "base/time/time.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

struct VR_UI_EXPORT Transition {
  Transition();
  ~Transition();

  base::TimeDelta duration;
  std::set<int> target_properties;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TRANSITION_H_
