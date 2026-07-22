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

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/kcer/kcer.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

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

// Logs the outcome of the best-effort managed identity cleanup that runs when
// the provisioning policy is disabled. `success` is false when the store failed
// to delete the persisted identities.
void LogManagedIdentityDeletion(const std::string& logging_context,
                                bool success);

void LogPrivateKeyCreationSource(const std::string& logging_context,
                                 PrivateKeySource source);

void LogLevelDBInitStatus(leveldb_proto::Enums::InitStatus status,
                          bool with_retry);

#if BUILDFLAG(IS_ANDROID)
void RecordClankKeySecurityLevel(BrowserKey::SecurityLevel security_level);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
// Records the `kcer::Error` reported when hardware-backed key generation fails
// and the ChromeOS client certificate provisioning flow falls back to a
// software key. Lets us track how often (and why) the TPM path is unavailable
// at an aggregate level.
void RecordKcerHardwareKeyGenerationError(kcer::Error error);

// Records the `kcer::Error` reported when tagging a freshly generated key as a
// browser enterprise client certificate key fails. Tagging is best-effort, so
// this tracks how often the ownership metadata ends up missing.
void RecordKcerKeyTaggingError(kcer::Error error);

// Records the `kcer::Error` reported when importing a client certificate into
// Kcer fails.
void RecordKcerCertificateImportError(kcer::Error error);

// Records the `kcer::Error` reported when removing a key (and its certificates)
// from Kcer fails. Covers both the SPKI-targeted deletion and the browser
// enterprise client certificate key sweep.
void RecordKcerKeyRemovalError(kcer::Error error);
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_METRICS_UTIL_H_
