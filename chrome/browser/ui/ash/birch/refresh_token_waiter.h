// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_REFRESH_TOKEN_WAITER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_REFRESH_TOKEN_WAITER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace ash {

// Waits for refresh tokens to be loaded for the primary account. These tokens
// are necessary to make network requests against Google APIs, e.g. to fetch
// weather or calendar events. Each instance of this class supports a single
// waiting client. If you need multiple waits, create multiple instances of the
// class.
class RefreshTokenWaiter : public signin::IdentityManager::Observer {
 public:
  explicit RefreshTokenWaiter(Profile* profile);
  RefreshTokenWaiter(const RefreshTokenWaiter&) = delete;
  RefreshTokenWaiter& operator=(const RefreshTokenWaiter&) = delete;
  ~RefreshTokenWaiter() override;

  // Waits for refresh tokens to be loaded then invokes `callback`. Will invoke
  // `callback` immediately if tokens are already loaded.
  void Wait(base::OnceClosure callback);

  // signin::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

 private:
  raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // Invoked when refresh tokens are loaded.
  base::OnceClosure callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_REFRESH_TOKEN_WAITER_H_
