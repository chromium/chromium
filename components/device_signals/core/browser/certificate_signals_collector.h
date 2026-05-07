// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_CERTIFICATE_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_CERTIFICATE_SIGNALS_COLLECTOR_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/device_signals/core/browser/base_signals_collector.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/base/net_errors.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace net {
class ClientCertIdentity;
class ClientCertStore;
}  // namespace net

namespace device_signals {

enum class SignalCollectionError;

// Collector in charge of collecting enterprise certificates on the device.
class CertificateSignalsCollector : public BaseSignalsCollector {
 public:
  using CertificateSignalsResponseCallback =
      base::RepeatingCallback<void(CertificateSignalsResponse)>;

  explicit CertificateSignalsCollector(
      std::unique_ptr<net::ClientCertStore> client_cert_store);
  ~CertificateSignalsCollector() override;

 private:
  // Collection function for the certificate signal. `request` contains the
  // details on which certificates should be collected. `response` will be
  // passed along and the signal values will be set on it when available.
  // `done_closure` will be invoked when signal collection is complete.
  void GetCertificateSignal(UserPermission permission,
                            const SignalsAggregationRequest& request,
                            SignalsAggregationResponse& response,
                            base::OnceClosure done_closure);

  // Callback invoked after the ClientCertStore retrieves certificates.
  void OnClientCertsRetrieved(
      base::TimeTicks start_time,
      std::vector<GetCertificateOptions> options,
      SignalsAggregationResponse& response,
      base::OnceClosure done_closure,
      std::vector<std::unique_ptr<net::ClientCertIdentity>> cert_identities);

  // Handles acquiring the private key, serializing data, and initiating
  // signing.
  void ProcessSingleCertificateAsync(
      std::unique_ptr<net::ClientCertIdentity> cert_identity,
      const std::string& nonce,
      int64_t timestamp,
      base::RepeatingCallback<
          void(std::optional<enterprise_management::SignedCertificateDetails>)>
          barrier_callback);

  // Callback invoked after the platform-specific utility signs the challenge.
  void OnCertificateDetailsSigned(
      std::vector<uint8_t> certificate_details,
      base::RepeatingCallback<
          void(std::optional<enterprise_management::SignedCertificateDetails>)>
          barrier_callback,
      net::Error error,
      const std::vector<uint8_t>& signature);

  // Aggregates all signed certificates once the barrier callback completes.
  // Modifies `response` with the final payload and invokes `done_closure`.
  void OnAllCertificatesProcessed(
      base::TimeTicks start_time,
      SignalsAggregationResponse& response,
      base::OnceClosure done_closure,
      bool truncated,
      std::vector<
          std::optional<enterprise_management::SignedCertificateDetails>>
          results);

  // Invoked when `certificate_signals_response` is collected. Updates the
  // `response` with the collected `certificate_signals_response` and invokes
  // the `done_closure` after.
  void OnSignalsCollected(
      base::TimeTicks start_time,
      SignalsAggregationResponse& response,
      base::OnceClosure done_closure,
      CertificateSignalsResponse certificate_signals_response);

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<net::ClientCertStore> client_cert_store_;
  base::WeakPtrFactory<CertificateSignalsCollector> weak_ptr_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_CERTIFICATE_SIGNALS_COLLECTOR_H_
