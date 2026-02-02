// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_IMMERSIVE_ACTIVATION_OBSERVER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_IMMERSIVE_ACTIVATION_OBSERVER_H_

#include "base/observer_list_types.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"

class ReadAnythingImmersiveActivationObserver : public base::CheckedObserver {
 public:
  virtual void OnShowImmersive(ReadAnythingOpenTrigger trigger) {}
  virtual void OnCloseImmersive() {}
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_IMMERSIVE_ACTIVATION_OBSERVER_H_
