// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_OBSERVER_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_OBSERVER_H_

#include "chrome/browser/ui/android/tab_model/tab_model.h"

// Observes possible changes to TabModelList.
class TabModelListObserver {
 public:
  virtual ~TabModelListObserver() = default;

  // Called after a TabModel is added.
  virtual void OnTabModelAdded(TabModel* tab_model) = 0;

  // Called after a TabModel is removed.
  virtual void OnTabModelRemoved(TabModel* tab_model) = 0;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_OBSERVER_H_
