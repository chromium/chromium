// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/mock_signing_service.h"

#include <string>

#include "components/policy/core/common/cloud/cloud_policy_client.h"

namespace em = enterprise_management;

namespace policy {

const char kSignedDataNonce[] = "+nonce";
const char kSignature[] = "fake-signature";

FakeSigningService::FakeSigningService() {}

FakeSigningService::~FakeSigningService() {}

void FakeSigningService::SignData(const std::string& data,
                                  SigningCallback callback) {
  em::SignedData signed_data;
  if (success_) {
    SignDataSynchronously(data, &signed_data);
  }
  std::move(callback).Run(success_, signed_data);
}

void FakeSigningService::SignDataSynchronously(const std::string& data,
    em::SignedData* signed_data) {
  signed_data->set_data(data + kSignedDataNonce);
  signed_data->set_signature(kSignature);
  signed_data->set_extra_data_bytes(sizeof(kSignedDataNonce) - 1);
}

void FakeSigningService::set_success(bool success) {
  success_ = success;
}

MockSigningService::MockSigningService() {}

MockSigningService::~MockSigningService() {}

} // namespace policy
