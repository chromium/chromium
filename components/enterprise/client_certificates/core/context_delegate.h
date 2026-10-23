// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CONTEXT_DELEGATE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CONTEXT_DELEGATE_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"

class PrefService;

namespace net {
class X509Certificate;
}  // namespace net

namespace client_certificates {

// Maximum length in UTF-8 bytes for a certificate Subject Common Name (CN)
// per the Google Cloud CAS limit (RFC 5280 allows 64 characters).
inline constexpr size_t kMaxCommonNameLength = 64;

// Returns true if `certificate` expires within the next `kDaysBeforeExpiration`
// days.
bool IsCertExpiringSoon(const net::X509Certificate& certificate);

// Sanitizes `raw_name` by stripping control/formatting characters (and invalid
// UTF-8 sequences) and leading/trailing whitespace, and truncating to at most
// `kMaxCommonNameLength` UTF-8 bytes on a valid UTF-8 code-point boundary.
std::string SanitizeAndTruncateCommonName(std::string_view raw_name);

// Delegate used to execute logic within a specific scope (e.g. a whole Browser,
// a specific Profile).
class ContextDelegate {
 public:
  ContextDelegate();
  virtual ~ContextDelegate();

  // Will notify the rest of the current context that `certificate` should no
  // longer be used and be considered as deleted.
  virtual void OnClientCertificateDeleted(
      scoped_refptr<net::X509Certificate> certificate) = 0;

  // Returns the expected Subject Common Name (CN) for the current context's
  // client certificate, or an empty string if the context's identity could not
  // be determined.
  virtual std::string GetExpectedCommonName() const = 0;

  // Returns whether `certificate` should be renewed for the current context.
  // Checks whether the certificate is expired or expiring soon, and validates
  // its Subject Common Name against `expected_common_name` if
  // `kClientCertCommonNameAttribute` is enabled.
  bool ShouldCertificateBeRenewed(const net::X509Certificate& certificate,
                                  std::string_view expected_common_name);

  // Returns a identity name for the current context.
  virtual std::string GetIdentityName() = 0;

  // Returns a temporary identity name for the current context.
  virtual std::string GetTemporaryIdentityName() = 0;

  // Returns the pref associated with a policy for the current context.
  virtual std::string GetPolicyPref() = 0;

  // Returns the pref associated with the configuration policy for the current
  // context, or an empty string if none is configured.
  virtual std::string GetConfigPolicyPref() const;

  // Returns the logging context used by histogram variants.
  virtual std::string GetLoggingContext() = 0;

  void SetComputerNameForTesting(std::optional<std::string> computer_name);

 protected:
  // If `kClientCertCommonNameAttribute` is enabled and `prefs` configures
  // `GetConfigPolicyPref()` with `common_name_source` set to `"computer_name"`,
  // returns the sanitized device hostname (or `std::nullopt` if the sanitized
  // hostname is empty, or on ChromeOS where computer name is not supported).
  std::optional<std::string> GetConfiguredComputerName(
      const PrefService* prefs) const;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  std::optional<std::string> computer_name_for_testing_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mutable std::optional<std::string> cached_computer_name_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CONTEXT_DELEGATE_H_
