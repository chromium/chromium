// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_SIGNIN_ENTERPRISE_IDENTITY_SERVICE_H_
#define COMPONENTS_ENTERPRISE_SIGNIN_ENTERPRISE_IDENTITY_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"

struct CoreAccountInfo;

namespace signin {
class IdentityManager;
}

namespace enterprise {

class EnterpriseIdentityService : public KeyedService {
 public:
  static std::unique_ptr<EnterpriseIdentityService> Create(
      signin::IdentityManager* identity_manager);

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // Invoked when a managed account's session state has changed (e.g. account
    // added, refresh tokens updated).
    virtual void OnManagedAccountSessionChanged() {}

   protected:
    Observer() = default;
  };

  using GetManagedAccountsCallback =
      base::OnceCallback<void(std::vector<CoreAccountInfo>)>;

  // Will invoke `callback` with the list of valid managed accounts. Makes sure
  // to wait for the extended account information to be available when needed
  // before resolving the callback.
  virtual void GetManagedAccountsWithRefreshTokens(
      GetManagedAccountsCallback callback) = 0;

  // Will invoke `callback` with a list of OAuth access tokens created with the
  // DM server scope for each valid managed accounts.
  virtual void GetManagedAccountsAccessTokens(
      base::OnceCallback<void(std::vector<std::string>)> callback) = 0;

  // Adds `observer` to the list of observers.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes `observer` from the list of observers.
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_SIGNIN_ENTERPRISE_IDENTITY_SERVICE_H_
