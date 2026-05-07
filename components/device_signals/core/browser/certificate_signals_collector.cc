// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/certificate_signals_collector.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"

namespace device_signals {

namespace em = enterprise_management;

CertificateSignalsCollector::CertificateSignalsCollector(
    std::unique_ptr<net::ClientCertStore> client_cert_store)
    : BaseSignalsCollector({
          {SignalName::kCertificates,
           base::BindRepeating(
               &CertificateSignalsCollector::GetCertificateSignal,
               base::Unretained(this))},
      }),
      client_cert_store_(std::move(client_cert_store)) {
  CHECK(client_cert_store_);
}

CertificateSignalsCollector::~CertificateSignalsCollector() = default;

void CertificateSignalsCollector::GetCertificateSignal(
    UserPermission permission,
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  // TODO(b/502634772): Implement.
  NOTIMPLEMENTED();
}

void CertificateSignalsCollector::OnClientCertsRetrieved(
    base::TimeTicks start_time,
    std::vector<GetCertificateOptions> options,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    std::vector<std::unique_ptr<net::ClientCertIdentity>> cert_identities) {
  // TODO(b/502634772): Implement.
  NOTIMPLEMENTED();
}

void CertificateSignalsCollector::ProcessSingleCertificateAsync(
    std::unique_ptr<net::ClientCertIdentity> cert_identity,
    const std::string& nonce,
    int64_t timestamp,
    base::RepeatingCallback<
        void(std::optional<enterprise_management::SignedCertificateDetails>)>
        barrier_callback) {
  // TODO(b/502634772): Implement.
  NOTIMPLEMENTED();
}

void CertificateSignalsCollector::OnCertificateDetailsSigned(
    std::vector<uint8_t> certificate_details,
    base::RepeatingCallback<
        void(std::optional<enterprise_management::SignedCertificateDetails>)>
        barrier_callback,
    net::Error error,
    const std::vector<uint8_t>& signature) {
  // TODO(b/502634772): Implement.
  NOTIMPLEMENTED();
}

void CertificateSignalsCollector::OnAllCertificatesProcessed(
    base::TimeTicks start_time,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    bool truncated,
    std::vector<std::optional<enterprise_management::SignedCertificateDetails>>
        results) {
  // TODO(b/502634772): Implement.
  NOTIMPLEMENTED();
}

void CertificateSignalsCollector::OnSignalsCollected(
    base::TimeTicks start_time,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    CertificateSignalsResponse certificate_signals_response) {
  // TODO(b/502634772): Implement.
  NOTIMPLEMENTED();
}

}  // namespace device_signals
