// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/device_sync/cryptauth_enroller.h"

namespace ash {

namespace device_sync {

class CryptAuthClientFactory;

// CryptAuthEnrollerFactory implementation which utilizes IdentityManager.
class CryptAuthEnrollerFactoryImpl : public CryptAuthEnrollerFactory {
 public:
  CryptAuthEnrollerFactoryImpl(
      CryptAuthClientFactory* cryptauth_client_factory);
  ~CryptAuthEnrollerFactoryImpl() override;

  // CryptAuthEnrollerFactory:
  std::unique_ptr<CryptAuthEnroller> CreateInstance() override;

 private:
  raw_ptr<CryptAuthClientFactory> cryptauth_client_factory_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H_
