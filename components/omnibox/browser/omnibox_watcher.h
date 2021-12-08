// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_WATCHER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_WATCHER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"

#if !defined(OS_IOS)
#include "content/public/browser/browser_context.h"
#endif  // !defined(OS_IOS)

// This KeyedService is meant to observe the omnibox and provide notifications
// to observers on suggestion changes and provided input.
class OmniboxWatcher : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Notifies listeners that omnibox input has been entered.
    virtual void OnOmniboxInputEntered() {}
  };

#if !defined(OS_IOS)
  static OmniboxWatcher* GetForBrowserContext(
      content::BrowserContext* browser_context);
#endif  // !defined(OS_IOS)

  OmniboxWatcher();
  ~OmniboxWatcher() override;
  OmniboxWatcher(const OmniboxWatcher&) = delete;
  OmniboxWatcher& operator=(const OmniboxWatcher&) = delete;

  void NotifyInputEntered();

  // Add/remove observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_WATCHER_H_
