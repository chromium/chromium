// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_OBSERVER_H_
#define CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_OBSERVER_H_

#include "base/observer_list_types.h"

class GlobalErrorObserver : public base::CheckedObserver {
 public:
  // Called whenever the set of GlobalErrors has changed.
  virtual void OnGlobalErrorsChanged() = 0;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_OBSERVER_H_
