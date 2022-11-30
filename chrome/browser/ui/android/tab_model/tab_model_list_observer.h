// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_OBSERVER_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_OBSERVER_H_

// Observes possible changes to TabModelList.
class TabModelListObserver {
 public:
  virtual ~TabModelListObserver() {}

  // Called after a TabModel is added.
  virtual void OnTabModelAdded() = 0;

  // Called after a TabModel is removed.
  virtual void OnTabModelRemoved() = 0;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_OBSERVER_H_
