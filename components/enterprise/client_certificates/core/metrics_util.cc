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
    case ProvisioningScenario::kExistingIdentity:
      return "ExistingIdentity";
  }
}

std::string_view SuccessToString(bool success) {
  return success ? "Success" : "Failure";
}

std::string_view WithRetryToString(bool with_retry) {
  return with_retry ? "WithRetry" : "NoRetry";
}

}  // namespace

void LogProvisioningError(const std::string& logging_context,
                          ProvisioningError provisioning_error,
                          std::optional<StoreError> store_error) {
  static constexpr char kProvisioningErrorHistogram[] =
      "Enterprise.ClientCertificate.%s.Provisioning.Error";
  base::UmaHistogramEnumeration(
      base::StringPrintf(kProvisioningErrorHistogram, logging_context.c_str()),
      provisioning_error);

  if (store_error.has_value()) {
    static constexpr char kProvisioningStoreErrorHistogram[] =
        "Enterprise.ClientCertificate.%s.Provisioning.Store.Error";
    base::UmaHistogramEnumeration(
        base::StringPrintf(kProvisioningStoreErrorHistogram,
                           logging_context.c_str()),
        store_error.value());
  }
}

void LogCertificateCreationResponse(const std::string& logging_context,
                                    HttpCodeOrClientError upload_code,
                                    bool has_certificate) {
  if (!upload_code.has_value()) {
    static constexpr char kCreateCertificateClientErrorHistogram[] =
        "Enterprise.ClientCertificate.%s.CreateCertificate.ClientError";
    base::UmaHistogramEnumeration(
        base::StringPrintf(kCreateCertificateClientErrorHistogram,
                           logging_context.c_str()),
        upload_code.error());
    return;
  }

  static constexpr char kCreateCertificateCodeHistogram[] =
      "Enterprise.ClientCertificate.%s.CreateCertificate.UploadCode";
  base::UmaHistogramSparse(base::StringPrintf(kCreateCertificateCodeHistogram,
                                              logging_context.c_str()),
                           upload_code.value());

  if (upload_code.value() / 100 == 2) {
    static constexpr char kCreateCertificateSuccessHasCertHistogram[] =
        "Enterprise.ClientCertificate.%s.CreateCertificate.Success."
        "HasCert";
    base::UmaHistogramBoolean(
        base::StringPrintf(kCreateCertificateSuccessHasCertHistogram,
                           logging_context.c_str()),
        has_certificate);
  }
}

void LogProvisioningContext(const std::string& logging_context,
                            ProvisioningContext context,
                            bool success) {
  static constexpr char kProvisioningOutcomeHistogramFormat[] =
      "Enterprise.ClientCertificate.%s.Provisioning.%s.Outcome";
  static constexpr char kProvisioningLatencyHistogramFormat[] =
      "Enterprise.ClientCertificate.%s.Provisioning.%s.%s.Latency";

  auto scenario_string = ProvisioningScenarioToString(context.scenario);
  base::UmaHistogramBoolean(
      base::StringPrintf(kProvisioningOutcomeHistogramFormat,
                         logging_context.c_str(), scenario_string.data()),
      success);
  base::UmaHistogramTimes(
      base::StringPrintf(kProvisioningLatencyHistogramFormat,
                         logging_context.c_str(), scenario_string.data(),
                         SuccessToString(success).data()),
      base::TimeTicks::Now() - context.start_time);
}

void LogPrivateKeyCreationSource(const std::string& logging_context,
                                 PrivateKeySource source) {
  static constexpr char kCreatePrivateKeySourceHistogram[] =
      "Enterprise.ClientCertificate.%s.CreatePrivateKey.Source";
  base::UmaHistogramEnumeration(
      base::StringPrintf(kCreatePrivateKeySourceHistogram,
                         logging_context.c_str()),
      source);
}

void LogLevelDBInitStatus(leveldb_proto::Enums::InitStatus status,
                          bool with_retry) {
  static constexpr char kLevelDBInitHistogramFormat[] =
      "Enterprise.CertificateStore.LevelDB.InitStatus.%s";
  base::UmaHistogramSparse(
      base::StringPrintf(kLevelDBInitHistogramFormat,
                         WithRetryToString(with_retry).data()),
      status);
}

#if BUILDFLAG(IS_ANDROID)
void RecordClankKeySecurityLevel(BrowserKey::SecurityLevel security_level) {
  static constexpr char kClankKeySecurityLevelHistogram[] =
      "Enterprise.ClientCertificates.ClankKeySecurityLevel";
  base::UmaHistogramEnumeration(kClankKeySecurityLevelHistogram,
                                security_level);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace client_certificates
