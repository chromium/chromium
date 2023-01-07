// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_ACTIVE_STATE_MANAGER_IMPL_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_ACTIVE_STATE_MANAGER_IMPL_H_

#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/signin/ios/browser/active_state_manager.h"

namespace web {
class BrowserState;
}  // namespace web

// Concrete subclass of web::ActiveStateManager. Informs observers when an
// ActiveStateManager becomes active/inactive.
class ActiveStateManagerImpl : public ActiveStateManager,
                               public base::SupportsUserData::Data {
 public:
  explicit ActiveStateManagerImpl(web::BrowserState* browser_state);

  ActiveStateManagerImpl(const ActiveStateManagerImpl&) = delete;
  ActiveStateManagerImpl& operator=(const ActiveStateManagerImpl&) = delete;

  ~ActiveStateManagerImpl() override;

  // ActiveStateManager methods.
  void SetActive(bool active) override;
  bool IsActive() override;
  void AddObserver(ActiveStateManager::Observer* observer) override;
  void RemoveObserver(ActiveStateManager::Observer* observer) override;

 private:
  web::BrowserState* browser_state_;  // weak, owns this object.
  // true if the ActiveStateManager is active.
  bool active_;
  // The list of observers.
  base::ObserverList<Observer>::Unchecked observer_list_;
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_ACTIVE_STATE_MANAGER_IMPL_H_
