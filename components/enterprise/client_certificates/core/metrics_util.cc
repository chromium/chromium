// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace client_certificates {

namespace {

std::string_view ProvisioningScenarioToString(ProvisioningScenario scenario) {
  switch (scenario) {
    case ProvisioningScenario::kUnknown:
      return "Unknown";
    case ProvisioningScenario::kCertificateCreation:
      return "CertificateCreation";
    case ProvisioningScenario::kCertificateRenewal:
      return "CertificateRenewal";
    case ProvisioningScenario::kPublicKeySync:
      return "PublicKeySync";
  }
}

std::string_view SuccessToString(bool success) {
  return success ? "Success" : "Failure";
}

}  // namespace

void LogProvisioningError(ProvisioningError provisioning_error,
                          std::optional<StoreError> store_error) {
  static constexpr char kProvisioningErrorHistogram[] =
      "Enterprise.ClientCertificate.Profile.Provisioning.Error";
  base::UmaHistogramEnumeration(kProvisioningErrorHistogram,
                                provisioning_error);

  if (store_error.has_value()) {
    static constexpr char kProvisioningStoreErrorHistogram[] =
        "Enterprise.ClientCertificate.Profile.Provisioning.Store.Error";
    base::UmaHistogramEnumeration(kProvisioningStoreErrorHistogram,
                                  store_error.value());
  }
}

void LogKeySyncResponse(HttpCodeOrClientError upload_code) {
  if (upload_code.has_value()) {
    static constexpr char kKeySyncCodeHistogram[] =
        "Enterprise.ClientCertificate.Profile.PublicKeySync.UploadCode";
    base::UmaHistogramSparse(kKeySyncCodeHistogram, upload_code.value());
    return;
  }

  static constexpr char kKeySyncClientErrorHistogram[] =
      "Enterprise.ClientCertificate.Profile.PublicKeySync.ClientError";
  base::UmaHistogramEnumeration(kKeySyncClientErrorHistogram,
                                upload_code.error());
}

void LogCertificateCreationResponse(HttpCodeOrClientError upload_code,
                                    bool has_certificate) {
  if (!upload_code.has_value()) {
    static constexpr char kCreateCertificateClientErrorHistogram[] =
        "Enterprise.ClientCertificate.Profile.CreateCertificate.ClientError";
    base::UmaHistogramEnumeration(kCreateCertificateClientErrorHistogram,
                                  upload_code.error());
    return;
  }

  static constexpr char kCreateCertificateCodeHistogram[] =
      "Enterprise.ClientCertificate.Profile.CreateCertificate.UploadCode";
  base::UmaHistogramSparse(kCreateCertificateCodeHistogram,
                           upload_code.value());

  if (upload_code.value() / 100 == 2) {
    static constexpr char kCreateCertificateSuccessHasCertHistogram[] =
        "Enterprise.ClientCertificate.Profile.CreateCertificate.Success."
        "HasCert";
    base::UmaHistogramBoolean(kCreateCertificateSuccessHasCertHistogram,
                              has_certificate);
  }
}

void LogProvisioningContext(ProvisioningContext context, bool success) {
  static constexpr char kProvisioningOutcomeHistogramFormat[] =
      "Enterprise.ClientCertificate.Profile.Provisioning.%s.Outcome";
  static constexpr char kProvisioningLatencyHistogramFormat[] =
      "Enterprise.ClientCertificate.Profile.Provisioning.%s.%s.Latency";

  auto scenario_string = ProvisioningScenarioToString(context.scenario);
  base::UmaHistogramBoolean(
      base::StringPrintf(kProvisioningOutcomeHistogramFormat,
                         scenario_string.data()),
      success);
  base::UmaHistogramTimes(
      base::StringPrintf(kProvisioningLatencyHistogramFormat,
                         scenario_string.data(),
                         SuccessToString(success).data()),
      base::TimeTicks::Now() - context.start_time);
}

void LogPrivateKeyCreationSource(PrivateKeySource source) {
  static constexpr char kCreatePrivateKeySourceHistogram[] =
      "Enterprise.ClientCertificate.Profile.CreatePrivateKey.Source";
  base::UmaHistogramEnumeration(kCreatePrivateKeySourceHistogram, source);
}

}  // namespace client_certificates
