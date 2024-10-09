// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_APP_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_APP_CLIENT_H_

#include <map>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace ash::boca {

// Defines the interface for sub features to access hub Events
class BocaAppClient : public signin::IdentityManager::Observer {
 public:
  BocaAppClient(const BocaAppClient&) = delete;
  BocaAppClient& operator=(const BocaAppClient&) = delete;

  static BocaAppClient* Get();

  static bool HasInstance();

  // Returns the IdentityManager for the active user profile.
  virtual signin::IdentityManager* GetIdentityManager() = 0;

  // Returns the URLLoaderFactory associated with user profile.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

  // Add `BocaSessionManager` instance for the current profile.
  virtual void AddSessionManager(BocaSessionManager* session_manager);

  // Get `BocaSessionManager` instance for the current profile.
  virtual BocaSessionManager* GetSessionManager();

  // Get virtual device id. Returns empty is device is not enrolled and has no
  // device policy.
  virtual std::string GetDeviceId();

  // IdentityManager overrides.
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 protected:
  BocaAppClient();
  ~BocaAppClient() override;

 private:
  std::map<signin::IdentityManager*, BocaSessionManager*> session_manager_map_;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_APP_CLIENT_H_
