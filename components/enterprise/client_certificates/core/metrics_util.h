// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_METRICS_UTIL_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_METRICS_UTIL_H_

#include <optional>

#include "base/time/time.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/store_error.h"
#include "components/enterprise/client_certificates/core/upload_client_error.h"
#include "components/leveldb_proto/public/proto_database.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/enterprise/client_certificates/android/browser_binding/browser_key.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace client_certificates {

// Captures terminal failure states of the certificate provisioning flow. Do not
// reorder values as they are used in histograms logging
// (CertificateProvisioningError in enums.xml).
enum class ProvisioningError {
  kIdentityLoadingFailed = 0,
  kTemporaryIdentityLoadingFailed = 1,
  kMissingPrivateKey = 2,
  kMissingTemporaryPrivateKey = 3,
  kPrivateKeyCreationFailed = 4,
  kCertificateCreationFailed = 5,
  kCertificateCommitFailed = 6,
  kMaxValue = kCertificateCommitFailed
};

enum class ProvisioningScenario {
  kUnknown = 0,
  kCertificateCreation = 1,
  kCertificateRenewal = 2,
  kExistingIdentity = 3,
};

struct ProvisioningContext {
  const base::TimeTicks start_time{base::TimeTicks::Now()};
  ProvisioningScenario scenario{ProvisioningScenario::kUnknown};
};

void LogProvisioningError(const std::string& logging_context,
                          ProvisioningError provisioning_error,
                          std::optional<StoreError> store_error);

void LogCertificateCreationResponse(const std::string& logging_context,
                                    HttpCodeOrClientError upload_code,
                                    bool has_certificate);

void LogProvisioningContext(const std::string& logging_context,
                            ProvisioningContext context,
                            bool success);

void LogPrivateKeyCreationSource(const std::string& logging_context,
                                 PrivateKeySource source);

void LogLevelDBInitStatus(leveldb_proto::Enums::InitStatus status,
                          bool with_retry);

#if BUILDFLAG(IS_ANDROID)
void RecordClankKeySecurityLevel(BrowserKey::SecurityLevel security_level);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_METRICS_UTIL_H_
