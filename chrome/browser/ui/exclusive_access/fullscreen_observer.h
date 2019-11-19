// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_OBSERVER_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_OBSERVER_H_

#include "base/observer_list_types.h"

// An interface to be notified of changes in FullscreenController.
class FullscreenObserver : public base::CheckedObserver {
 public:
  virtual void OnFullscreenStateChanged() = 0;
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_OBSERVER_H_
