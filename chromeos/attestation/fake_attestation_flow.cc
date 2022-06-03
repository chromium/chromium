// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/attestation/fake_attestation_flow.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "components/account_id/account_id.h"

namespace chromeos {
namespace attestation {

namespace {

constexpr char kFakeCert[] = "cert";

}  // namespace

// This constructor passes |nullptr|s to the base class because we don't use
// server proxy in |AttestationFlowIntegrated|.
//
// TOOD(b/158955123): Remove this transitional state along with the removal of
// |AttestationFlow|.
FakeAttestationFlow::FakeAttestationFlow()
    : AttestationFlow(/*server_proxy=*/nullptr) {}

FakeAttestationFlow::~FakeAttestationFlow() = default;

void FakeAttestationFlow::GetCertificate(
    AttestationCertificateProfile /*certificate_profile*/,
    const AccountId& /*account_id*/,
    const std::string& /*request_origin*/,
    bool /*force_new_key*/,
    const std::string& /*key_name*/,
    CertificateCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     AttestationStatus::ATTESTATION_SUCCESS, kFakeCert));
}

}  // namespace attestation
}  // namespace chromeos
