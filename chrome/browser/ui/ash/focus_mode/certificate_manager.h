// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_FOCUS_MODE_CERTIFICATE_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_FOCUS_MODE_CERTIFICATE_MANAGER_H_

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"

namespace ash {

namespace attestation {
class AttestationFlow;
}  // namespace attestation

class AttestationClient;

}  // namespace ash

// Manages certificates from AttestationService for request signing with a
// Content Protection Certificate.
class CertificateManager {
 public:
  // An opaque identifier for certificates to ensure that certificate from
  // `GetCertificate()` can be used for `Sign()`.
  struct Key {
    explicit Key(const std::string& label, base::Time expiration);
    Key(const Key&);

    bool operator==(const Key&);

    // Name of the certificate in attestationd. Used to lookup the key for
    // future signing requests.
    const std::string label;

    // The expiration time of the certificate. Certificates should be renewed
    // before they expire or they can be rejected by the server.
    const base::Time expiration;
  };

  enum class CertificateResult {
    kSuccess,
    kDisallowedByPolicy,
    kInvalidKey,
    kCertificateExpired
  };

  // `expiration_buffer` defines the time allowed before a certificate is
  // considered expired. If a certificate expires in less than
  // `expiration_buffer`, a new certificate will be retrieved. Otherwise, the
  // cached certificate will be provided.
  static std::unique_ptr<CertificateManager> Create(
      const AccountId& account_id,
      base::TimeDelta expiration_buffer);

  // Create a `CertificateManager` for testing where dependencies can be
  // replaced with fakes.
  static std::unique_ptr<CertificateManager> CreateForTesting(
      const AccountId& account_id,
      base::TimeDelta expiration_buffer,
      std::unique_ptr<ash::attestation::AttestationFlow> attestation_flow,
      ash::AttestationClient* attestation_client);

  CertificateManager() = default;
  CertificateManager(const CertificateManager&) = delete;
  CertificateManager& operator=(const CertificateManager&) = delete;
  virtual ~CertificateManager() = default;

  // Attempts to retrieve a certificate via the attestation service. If
  // successful, returns an identifier for use with `Sign()`. If a previously
  // retrieved certificate is still valid, a new certificate will not be
  // retrieved.
  using CertificateCallback = base::OnceCallback<void(
      const std::optional<CertificateManager::Key>& certificate_key)>;
  virtual bool GetCertificate(bool force_update,
                              CertificateCallback callback) = 0;

  // Uses a previously obtained certificate that matches `certificate_key` to
  // sign `data`. Returns the signature for `data` as well as the certificates.
  // Returns false if the certificate is expired or signing is not supported on
  // the device.
  using SigningCallback = base::OnceCallback<void(
      bool success,
      const std::string& signature,
      const std::string& client_certificate,
      const std::vector<std::string>& intermediate_certificates)>;
  virtual CertificateResult Sign(const CertificateManager::Key& certificate_key,
                                 std::string_view data,
                                 SigningCallback callback) = 0;
};

#endif  // CHROME_BROWSER_UI_ASH_FOCUS_MODE_CERTIFICATE_MANAGER_H_
