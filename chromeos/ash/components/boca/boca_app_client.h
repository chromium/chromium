// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_APP_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_APP_CLIENT_H_

#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash::boca {

class SharedCrdSessionWrapper;

// Defines the interface for sub features to access hub Events
class BocaAppClient {
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

  // Launch Boca App.
  virtual void LaunchApp();

  // Returns the number of open app instances.
  virtual int GetAppInstanceCount();

  // Get virtual device id. Returns empty is device is not enrolled and has no
  // device policy.
  virtual std::string GetDeviceId();

  virtual std::string GetSchoolToolsServerBaseUrl();

  virtual void OpenFeedbackDialog();

  // TODO(crbug.com/447355422): Make it pure.
  // Gets a new `SharedCrdSessionWrapper` instance for the current profile.
  virtual std::unique_ptr<SharedCrdSessionWrapper>
  CreateSharedCrdSessionWrapper();

 protected:
  BocaAppClient();
  virtual ~BocaAppClient();
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_APP_CLIENT_H_
