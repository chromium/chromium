// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_enroller_factory_impl.h"

#include <memory>

#include "chromeos/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/services/device_sync/cryptauth_enroller_impl.h"

namespace chromeos {

namespace device_sync {

CryptAuthEnrollerFactoryImpl::CryptAuthEnrollerFactoryImpl(
    CryptAuthClientFactory* cryptauth_client_factory)
    : cryptauth_client_factory_(cryptauth_client_factory) {}

CryptAuthEnrollerFactoryImpl::~CryptAuthEnrollerFactoryImpl() = default;

std::unique_ptr<CryptAuthEnroller>
CryptAuthEnrollerFactoryImpl::CreateInstance() {
  return std::make_unique<CryptAuthEnrollerImpl>(
      cryptauth_client_factory_,
      multidevice::SecureMessageDelegateImpl::Factory::Create());
}

}  // namespace device_sync

}  // namespace chromeos
