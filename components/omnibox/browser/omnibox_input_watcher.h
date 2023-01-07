// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_INPUT_WATCHER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_INPUT_WATCHER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"

#if !BUILDFLAG(IS_IOS)
#include "content/public/browser/browser_context.h"
#endif  // !BUILDFLAG(IS_IOS)

// This KeyedService is meant to observe omnibox input and provide
// notifications.
//
// This watcher is part of the Omnibox Extensions API.
class OmniboxInputWatcher : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Notifies listeners that omnibox input has been entered.
    virtual void OnOmniboxInputEntered() {}
  };

#if !BUILDFLAG(IS_IOS)
  static OmniboxInputWatcher* GetForBrowserContext(
      content::BrowserContext* browser_context);
#endif  // !BUILDFLAG(IS_IOS)

  OmniboxInputWatcher();
  ~OmniboxInputWatcher() override;
  OmniboxInputWatcher(const OmniboxInputWatcher&) = delete;
  OmniboxInputWatcher& operator=(const OmniboxInputWatcher&) = delete;

  void NotifyInputEntered();

  // Add/remove observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_INPUT_WATCHER_H_
