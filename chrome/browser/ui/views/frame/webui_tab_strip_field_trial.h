// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_FIELD_TRIAL_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_FIELD_TRIAL_H_

#include "base/no_destructor.h"

// Manages a synthetic field trial for the WebUI tab strip. The feature
// flag itself is controlled by an external field trial. This synthetic
// trial mirrors the flag's state, but is only recorded if the device is
// tablet mode-capable.
class WebUITabStripFieldTrial {
 public:
  // Should be called at least once early in initialization.
  static void RegisterFieldTrialIfNecessary();

  static bool DeviceIsTabletModeCapable();

 private:
  friend class base::NoDestructor<WebUITabStripFieldTrial>;

  // The field trial is registered on construction.
  // |RegisterFieldTrialIfNecessary()| constructs this class as a static
  // variable.
  WebUITabStripFieldTrial();
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_FIELD_TRIAL_H_
