// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_APP_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_APP_CLIENT_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash::boca {

// Defines the interface for sub features to access hub Events
class BocaAppClient {
 public:
  BocaAppClient(const BocaAppClient&) = delete;
  BocaAppClient& operator=(const BocaAppClient&) = delete;

  static BocaAppClient* Get();

  // Returns the IdentityManager for the active user profile.
  virtual signin::IdentityManager* GetIdentityManager() = 0;

  // Returns the URLLoaderFactory associated with user profile.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

 protected:
  BocaAppClient();
  virtual ~BocaAppClient();

};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_APP_CLIENT_H_
