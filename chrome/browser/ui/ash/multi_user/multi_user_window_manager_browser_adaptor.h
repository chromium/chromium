// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_BROWSER_ADAPTOR_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_BROWSER_ADAPTOR_H_

#include <map>
#include <memory>

#include "ash/multi_user/multi_user_window_manager_observer.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/account_id/account_id.h"

class Browser;

namespace ash {
}  // namespace ash

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class MultiUserWindowManager;
class MultiUserWindowManagerBrowserAdaptorTest;

// This class acts as a helper to keep ash's MultiUserWindowManager in sync with
// windows created in the browser. For example, this adds all browser windows
// to MultiUserWindowManager as well as all app windows. This class is only
// created if SessionControllerClient::IsMultiProfileAvailable() returns true.
class MultiUserWindowManagerBrowserAdaptor
    : public MultiUserWindowManagerObserver,
      public BrowserListObserver {
 public:
  // Constructs the adaptor for the given MultiUserWindowManager.
  // `multi_user_window_manager` must not be nullptr, and outlive this instance.
  explicit MultiUserWindowManagerBrowserAdaptor(
      MultiUserWindowManager* multi_user_window_manager);

  MultiUserWindowManagerBrowserAdaptor(
      const MultiUserWindowManagerBrowserAdaptor&) = delete;
  MultiUserWindowManagerBrowserAdaptor& operator=(
      const MultiUserWindowManagerBrowserAdaptor&) = delete;

  ~MultiUserWindowManagerBrowserAdaptor() override;

  void AddUser(const AccountId& account_id);

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

 private:
  class AppObserver;

  using AccountIdToAppWindowObserver =
      std::map<AccountId, std::unique_ptr<AppObserver>>;

  friend class MultiUserWindowManagerBrowserAdaptorTest;

  // MultiUserWindowManagerObserver:
  void OnWindowOwnerEntryChanged(aura::Window* window,
                                 const AccountId& account_id,
                                 bool was_minimized,
                                 bool teleported) override;
  void OnTransitionUserShelfToNewAccount() override;

  const raw_ref<MultiUserWindowManager> multi_user_window_manager_;
  base::ScopedObservation<MultiUserWindowManager,
                          MultiUserWindowManagerObserver>
      multi_user_window_manager_observation_{this};

  // A list of all known users and their app window observers.
  AccountIdToAppWindowObserver account_id_to_app_observer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_BROWSER_ADAPTOR_H_
