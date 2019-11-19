// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_PASSWORD_RECOVERY_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_PASSWORD_RECOVERY_MANAGER_H_

#include <string>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/win/windows_types.h"
#include "url/gurl.h"

namespace credential_provider {

// Manager used to handle requests to store an encrypted recovery password for
// a given user and to retrieve this encrypted password.
class PasswordRecoveryManager {
 public:
  // Default timeout when trying to make requests to the EMM escrow service to
  // retrieve encryption key.
  static const base::TimeDelta kDefaultEscrowServiceEncryptionKeyRequestTimeout;

  // Default timeout when trying to make requests to the EMM escrow service to
  // retrieve decryption key.
  static const base::TimeDelta kDefaultEscrowServiceDecryptionKeyRequestTimeout;

  static PasswordRecoveryManager* Get();

  // Clear the password recovery information stored in the LSA for user with SID
  // |sid|.
  HRESULT ClearUserRecoveryPassword(const base::string16& sid);

  // Attempts to recover the password for user with SID |sid| using the EMM
  // escrow service.
  HRESULT RecoverWindowsPasswordIfPossible(const base::string16& sid,
                                           const std::string& access_token,
                                           base::string16* recovered_password);
  // Attempts to store encryped passwod information for user with SID |sid| in
  // the LSA.
  HRESULT StoreWindowsPasswordIfNeeded(const base::string16& sid,
                                       const std::string& access_token,
                                       const base::string16& password);

  // Calculates the full url of various escrow service requests based on
  // the registry setting for the escrow server url.
  GURL GetEscrowServiceGenerateKeyPairUrl();
  GURL GetEscrowServiceGetPrivateKeyUrl(const std::string& resource_id);

 protected:
  // Returns the storage used for the instance pointer.
  static PasswordRecoveryManager** GetInstanceStorage();

  explicit PasswordRecoveryManager(
      base::TimeDelta encryption_key_request_timeout,
      base::TimeDelta decryption_key_request_timeout);
  virtual ~PasswordRecoveryManager();

  void SetRequestTimeoutForTesting(base::TimeDelta request_timeout) {
    encryption_key_request_timeout_ = request_timeout;
    decryption_key_request_timeout_ = request_timeout;
  }
  std::string MakeGenerateKeyPairResponseForTesting(
      const std::string& public_key,
      const std::string& resource_id);
  std::string MakeGetPrivateKeyResponseForTesting(
      const std::string& private_key);

 private:
  base::TimeDelta encryption_key_request_timeout_;
  base::TimeDelta decryption_key_request_timeout_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_PASSWORD_RECOVERY_MANAGER_H_
