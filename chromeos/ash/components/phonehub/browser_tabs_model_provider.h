// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_BROWSER_TABS_MODEL_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_BROWSER_TABS_MODEL_PROVIDER_H_

#include <ostream>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model.h"

namespace ash {
namespace phonehub {

// Responsible for providing BrowserTabsModel information to observers.
class BrowserTabsModelProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnBrowserTabsUpdated(
        bool is_sync_enabled,
        const std::vector<BrowserTabsModel::BrowserTabMetadata>&
            browser_tabs_metadata) = 0;
  };

  BrowserTabsModelProvider(const BrowserTabsModelProvider&) = delete;
  BrowserTabsModelProvider* operator=(const BrowserTabsModelProvider&) = delete;
  virtual ~BrowserTabsModelProvider();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Used to manually request updates for the latest browser tab metadata,
  // instead of lazily waiting for an update to occur on its own. This may be
  // advantageous when the user opens the PhoneHub tray immediately after
  // visiting a link on the connected phone's Chrome Browser.
  virtual void TriggerRefresh() = 0;

  virtual bool IsBrowserTabSyncEnabled() = 0;

 protected:
  BrowserTabsModelProvider();

  void NotifyBrowserTabsUpdated(
      bool is_sync_enabled,
      const std::vector<BrowserTabsModel::BrowserTabMetadata>
          browser_tabs_metadata);

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_BROWSER_TABS_MODEL_PROVIDER_H_
