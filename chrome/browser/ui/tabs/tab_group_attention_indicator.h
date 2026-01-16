// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_ATTENTION_INDICATOR_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_ATTENTION_INDICATOR_H_

#include "base/observer_list.h"

// Manages the attention state of a tab group.
class TabGroupAttentionIndicator {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAttentionStateChanged() {}
  };

  TabGroupAttentionIndicator();
  ~TabGroupAttentionIndicator();

  bool GetHasAttention() const { return has_attention_; }
  void SetHasAttention(bool has_attention);

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

 private:
  bool has_attention_ = false;
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_ATTENTION_INDICATOR_H_
