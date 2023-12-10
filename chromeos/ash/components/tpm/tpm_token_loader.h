// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TPM_TPM_TOKEN_LOADER_H_
#define CHROMEOS_ASH_COMPONENTS_TPM_TPM_TOKEN_LOADER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/tpm/tpm_token_info_getter.h"

namespace base {
class SequencedTaskRunner;
}

namespace ash {

// This class is responsible for loading the TPM backed token for the system
// slot when the user logs in. It is expected to be constructed on the UI thread
// and public methods should all be called from the UI thread.
// When the TPM token is loaded, or if the TPM should stay disabled for the
// session, the observers are notified using |OnTPMTokenReady|.
// Note: This currently initializes the token with the hard coded default id 0.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_TPM) TPMTokenLoader
    : public LoginState::Observer {
 public:
  enum TPMTokenStatus {
    TPM_TOKEN_STATUS_UNDETERMINED,
    TPM_TOKEN_STATUS_ENABLED,
    TPM_TOKEN_STATUS_DISABLED
  };

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

  TPMTokenLoader(const TPMTokenLoader&) = delete;
  TPMTokenLoader& operator=(const TPMTokenLoader&) = delete;

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
      std::optional<user_data_auth::TpmTokenInfo> token_info);
  void OnTPMTokenInitialized(bool success);

  // Notifies observers that the TPM token is ready.
  void NotifyTPMTokenReady();

  // LoginState::Observer
  void LoggedInStateChanged() override;

  bool enable_tpm_loading_for_testing_ = false;

  bool initialized_for_test_;

  // The states are traversed in this order but some might get omitted or never
  // be left.
  enum TPMTokenState {
    TPM_STATE_UNKNOWN,
    TPM_INITIALIZATION_STARTED,
    TPM_TOKEN_INFO_RECEIVED,
    TPM_TOKEN_INITIALIZED,
    TPM_DISABLED,
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
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TPM_TPM_TOKEN_LOADER_H_
