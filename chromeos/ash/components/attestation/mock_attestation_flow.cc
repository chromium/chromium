// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/mock_attestation_flow.h"

#include <memory>

#include "components/account_id/account_id.h"

using testing::_;
using testing::DefaultValue;
using testing::Invoke;

namespace ash {
namespace attestation {

FakeServerProxy::FakeServerProxy() : result_(true) {}

FakeServerProxy::~FakeServerProxy() = default;

void FakeServerProxy::SendEnrollRequest(const std::string& request,
                                        DataCallback callback) {
  std::move(callback).Run(result_, enroll_response_.empty()
                                       ? request + "_response"
                                       : enroll_response_);
}

void FakeServerProxy::SendCertificateRequest(const std::string& request,
                                             DataCallback callback) {
  std::move(callback).Run(
      result_, cert_response_.empty() ? request + "_response" : cert_response_);
}

void FakeServerProxy::CheckIfAnyProxyPresent(ProxyPresenceCallback callback) {
  std::move(callback).Run(true);
}

MockServerProxy::MockServerProxy() {
  DefaultValue<PrivacyCAType>::Set(DEFAULT_PCA);
}

MockServerProxy::~MockServerProxy() = default;

void MockServerProxy::DeferToFake(bool success) {
  fake_.set_result(success);
  ON_CALL(*this, SendEnrollRequest(_, _))
      .WillByDefault(Invoke(&fake_, &FakeServerProxy::SendEnrollRequest));
  ON_CALL(*this, SendCertificateRequest(_, _))
      .WillByDefault(Invoke(&fake_, &FakeServerProxy::SendCertificateRequest));
}

MockObserver::MockObserver() = default;

MockObserver::~MockObserver() = default;

MockAttestationFlow::MockAttestationFlow() = default;

MockAttestationFlow::~MockAttestationFlow() = default;

}  // namespace attestation
}  // namespace ash
