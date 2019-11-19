// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_TPM_TPM_TOKEN_LOADER_H_
#define CHROMEOS_TPM_TPM_TOKEN_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/tpm/tpm_token_info_getter.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {

// This class is responsible for loading the TPM backed token for the system
// slot when the user logs in. It is expected to be constructed on the UI thread
// and public methods should all be called from the UI thread.
// When the TPM token is loaded, or if the TPM should stay disabled for the
// session, the observers are notified using |OnTPMTokenReady|.
// Note: This currently initializes the token with the hard coded default id 0.
// See CryptohomeClient::OnPkcs11GetTpmTokenInfo.
class COMPONENT_EXPORT(CHROMEOS_TPM) TPMTokenLoader
    : public LoginState::Observer {
 public:
  enum TPMTokenStatus {
    TPM_TOKEN_STATUS_UNDETERMINED,
    TPM_TOKEN_STATUS_ENABLED,
    TPM_TOKEN_STATUS_DISABLED
  };

  using TPMReadyCallback = base::OnceCallback<void(bool)>;
  using TPMReadyCallbackList = std::vector<TPMReadyCallback>;

  // Sets the global instance. Must be called before any calls to Get().
  // The global instance will immediately start observing |LoginState|.
  static void Initialize();

  // Sets the global, stubbed out with the already initialized token, instance.
  // To be used in tests.
  static void InitializeForTest();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called before this.
  static TPMTokenLoader* Get();

  // Returns true if the global instance has been initialized.
  static bool IsInitialized();

  // |crypto_task_runner| is the task runner that any synchronous crypto calls
  // should be made from, e.g. in Chrome this is the IO thread. Must be called
  // after the thread is started. When called, this will attempt to start TPM
  // token loading.
  void SetCryptoTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner>& crypto_task_runner);

  // Starts loading TPM system token, if not yet started. It should be called
  // if the system token has to be loaded before a user logs in. By default (if
  // |EnsureStarted| is not called) system token loading will start when the
  // login state changes to LOGGED_IN_ACTIVE.
  void EnsureStarted();

  // Checks if the TPM token is enabled. If the state is unknown, |callback|
  // will be called back once the TPM state is known.
  TPMTokenStatus IsTPMTokenEnabled(TPMReadyCallback callback);

  std::string tpm_user_pin() const { return tpm_user_pin_; }

  // Allows tests to enable the TPM token loading logic in this class.
  void enable_tpm_loading_for_testing(bool enable) {
    enable_tpm_loading_for_testing_ = enable;
  }

 private:
  explicit TPMTokenLoader(bool initialized_for_test);
  ~TPMTokenLoader() override;

  bool IsTPMLoadingEnabled() const;

  // Starts tpm token initialization if the user is logged in and the crypto
  // task runner is set.
  void MaybeStartTokenInitialization();

  // This is the cyclic chain of callbacks to initialize the TPM token.
  void ContinueTokenInitialization();
  void OnTPMTokenEnabledForNSS();
  void OnGotTpmTokenInfo(
      base::Optional<CryptohomeClient::TpmTokenInfo> token_info);
  void OnTPMTokenInitialized(bool success);

  // Notifies observers that the TPM token is ready.
  void NotifyTPMTokenReady();

  // LoginState::Observer
  void LoggedInStateChanged() override;

  bool enable_tpm_loading_for_testing_ = false;

  bool initialized_for_test_;

  TPMReadyCallbackList tpm_ready_callback_list_;

  // The states are traversed in this order but some might get omitted or never
  // be left.
  enum TPMTokenState {
    TPM_STATE_UNKNOWN,
    TPM_INITIALIZATION_STARTED,
    TPM_TOKEN_ENABLED_FOR_NSS,
    TPM_DISABLED,
    TPM_TOKEN_INFO_RECEIVED,
    TPM_TOKEN_INITIALIZED,
  };
  TPMTokenState tpm_token_state_;

  std::unique_ptr<TPMTokenInfoGetter> tpm_token_info_getter_;

  // Cached TPM token info.
  int tpm_token_slot_id_;
  std::string tpm_user_pin_;

  // Whether TPM system token loading may be started before user log in.
  // This will be true iff |EnsureStarted| was called.
  bool can_start_before_login_;

  base::ThreadChecker thread_checker_;

  // TaskRunner for crypto calls.
  scoped_refptr<base::SequencedTaskRunner> crypto_task_runner_;

  base::WeakPtrFactory<TPMTokenLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TPMTokenLoader);
};

}  // namespace chromeos

#endif  // CHROMEOS_TPM_TPM_TOKEN_LOADER_H_
