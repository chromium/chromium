// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_TPM_TPM_TOKEN_INFO_GETTER_H_
#define CHROMEOS_TPM_TPM_TOKEN_INFO_GETTER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "components/account_id/account_id.h"

namespace base {
class TaskRunner;
}

namespace chromeos {

// Class for getting a user or the system TPM token info from cryptohome during
// TPM token loading.
class COMPONENT_EXPORT(CHROMEOS_TPM) TPMTokenInfoGetter {
 public:
  using TpmTokenInfoCallback = base::OnceCallback<void(
      base::Optional<CryptohomeClient::TpmTokenInfo> token_info)>;

  // Factory method for TPMTokenInfoGetter for a user token.
  static std::unique_ptr<TPMTokenInfoGetter> CreateForUserToken(
      const AccountId& account_id,
      CryptohomeClient* cryptohome_client,
      const scoped_refptr<base::TaskRunner>& delayed_task_runner);

  // Factory method for TPMTokenGetter for the system token.
  static std::unique_ptr<TPMTokenInfoGetter> CreateForSystemToken(
      CryptohomeClient* cryptohome_client,
      const scoped_refptr<base::TaskRunner>& delayed_task_runner);

  ~TPMTokenInfoGetter();

  // Starts getting TPM token info. Should be called at most once.
  // |callback| will be called when all the info is fetched.
  // The object may get deleted before |callback| is called, which is equivalent
  // to cancelling the info getting (in which case |callback| will never get
  // called).
  void Start(TpmTokenInfoCallback callback);

 private:
  enum Type {
    TYPE_SYSTEM,
    TYPE_USER
  };

  enum State {
    STATE_INITIAL,
    STATE_STARTED,
    STATE_TPM_ENABLED,
    STATE_DONE
  };

  TPMTokenInfoGetter(
      Type type,
      const AccountId& account_id,
      CryptohomeClient* cryptohome_client,
      const scoped_refptr<base::TaskRunner>& delayed_task_runner);

  // Continues TPM token info getting procedure by starting the task associated
  // with the current TPMTokenInfoGetter state.
  void Continue();

  // If token initialization step fails (e.g. if tpm token is not yet ready)
  // schedules the initialization step retry attempt after a timeout.
  void RetryLater();

  // Cryptohome methods callbacks.
  void OnTpmIsEnabled(base::Optional<bool> tpm_is_enabled);
  void OnPkcs11GetTpmTokenInfo(
      base::Optional<CryptohomeClient::TpmTokenInfo> token_info);

  // The task runner used to run delayed tasks when retrying failed Cryptohome
  // calls.
  scoped_refptr<base::TaskRunner> delayed_task_runner_;

  Type type_;
  State state_;

  // The account id associated with the TPMTokenInfoGetter. Empty for system
  // token.
  AccountId account_id_;

  TpmTokenInfoCallback callback_;

  // The current request delay before the next attempt to initialize the
  // TPM. Will be adapted after each attempt.
  base::TimeDelta tpm_request_delay_;

  CryptohomeClient* cryptohome_client_;

  base::WeakPtrFactory<TPMTokenInfoGetter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TPMTokenInfoGetter);
};

}  // namespace chromeos

#endif  // CHROMEOS_TPM_TPM_TOKEN_INFO_GETTER_H_
