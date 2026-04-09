// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TAB_MODEL_EVENT_BRIDGE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TAB_MODEL_EVENT_BRIDGE_H_

#include "base/observer_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/event_bridge.h"

namespace tabs_api {

class AndroidTabModelEventBridge : public EventBridge, public TabModelObserver {
 public:
  explicit AndroidTabModelEventBridge(TabModel* model);
  ~AndroidTabModelEventBridge() override;

  // EventBridge:
  void AddObserver(events::EventObserver* observer) override;
  void RemoveObserver(events::EventObserver* observer) override;

  // TabModelObserver:
  void DidRemoveTabForClosure(TabAndroid* tab) override;

 private:
  void Notify(events::Event event) const;

  base::ObserverList<events::EventObserver> observers_;
  raw_ptr<TabModel> model_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TAB_MODEL_EVENT_BRIDGE_H_
