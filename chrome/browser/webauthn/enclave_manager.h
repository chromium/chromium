// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_H_
#define CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_H_

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/types/strong_alias.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "device/fido/enclave/types.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace cbor {
class Value;
}

namespace crypto {
class UnexportableSigningKey;
class UserVerifyingKeyProvider;
class UserVerifyingSigningKey;
}  // namespace crypto

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace trusted_vault {
enum class TrustedVaultRegistrationStatus;
}

namespace webauthn_pb {
class EnclaveLocalState;
class EnclaveLocalState_User;
}  // namespace webauthn_pb

// EnclaveManager stores and manages the passkey enclave state. One instance
// exists per-profile, owned by `EnclaveManagerFactory`.
//
// The state exposed from this class is per-primary-account. This class watches
// the `IdentityManager` and, when the primary account changes, the result of
// functions like `is_registered` will suddenly change too. If an account is
// removed from the cookie jar (and it's not primary) then state for that
// account will be erased.
//
// Calling `Start` for the first time will cause the persisted state to be read
// from the disk. Each time all requested operations have completed, the class
// becomes "idle": `is_idle` will return true, and `OnEnclaveManagerIdle`
// will be called for all observers.
//
// When `is_ready` is true then this class can produce wrapped security domain
// secrets and signing callbacks to use to perform passkey operations with the
// enclave, which is the ultimate point of this class.
class EnclaveManager : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEnclaveManagerIdle() = 0;
  };

  EnclaveManager(
      const base::FilePath& base_dir,
      signin::IdentityManager* identity_manager,
      raw_ptr<network::mojom::NetworkContext> network_context,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~EnclaveManager() override;
  EnclaveManager(const EnclaveManager&) = delete;
  EnclaveManager(const EnclaveManager&&) = delete;

  // Returns true if there are no current operations pending.
  bool is_idle() const;
  // Returns true if the persistent state has been loaded from the disk. (Or
  // else the loading failed and an empty state is being used.)
  bool is_loaded() const;
  // Returns true if the current user has been registered with the enclave.
  bool is_registered() const;
  // Returns true if the current user has joined the security domain and has one
  // or more wrapped security domain secrets available. (This implies
  // `is_registered`.)
  bool is_ready() const;
  // Returns the number of times that `StoreKeys` has been called.
  unsigned store_keys_count() const;
  // Returns true when a UV signing key has been configured.
  bool is_uv_key_available() const;

  // Start by loading the persisted state from disk. Harmless to call multiple
  // times.
  void Start();
  // Register with the enclave if not already registered.
  void RegisterIfNeeded();

  // Get a callback to sign with the registered "hw" key. Only valid to call if
  // `is_ready`.
  device::enclave::SigningCallback HardwareKeySigningCallback();
  // Get a callback to sign with the registered "uv" key. Only valid to call if
  // `is_ready`.
  device::enclave::SigningCallback UserVerifyingKeySigningCallback();
  // Fetch a wrapped security domain secret for the given epoch. Only valid to
  // call if `is_ready`.
  std::optional<std::vector<uint8_t>> GetWrappedSecret(int32_t version);
  // Fetch all wrapped security domain secrets, for when it's unknown which one
  // a WebauthnCredentialSpecifics will need. Only valid to call if `is_ready`.
  std::vector<std::vector<uint8_t>> GetWrappedSecrets();
  // Get the version and value of the current wrapped secret. Only valid to call
  // if `is_ready`.
  std::pair<int32_t, std::vector<uint8_t>> GetCurrentWrappedSecret();

  // Get an access token for contacting the enclave.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> GetAccessToken(
      base::OnceCallback<void(std::optional<std::string>)> callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void StoreKeys(const std::string& gaia_id,
                 std::vector<std::vector<uint8_t>> keys,
                 int last_key_version);

  // If background processes need to be stopped then return true and call
  // `on_stop` when stopped. Otherwise return false.
  bool RunWhenStoppedForTesting(base::OnceClosure on_stop);

  const webauthn_pb::EnclaveLocalState& local_state_for_testing() const;

 private:
  struct StoreKeysArgs;
  class IdentityObserver;

  // The main part of this class is a state machine that uses the following
  // states. It moves from state to state in response to `Event` values.
  // Fields such as `want_registration_` and `identity_updated_` are set in
  // order to record that the state machine needs to process those requests
  // once the current processing has completed.
  enum class State {
    kInit,
    kIdle,
    kNextAction,
    kLoading,
    kGeneratingKeys,
    kWaitingForEnclaveTokenForRegistration,
    kRegisteringWithEnclave,
    kWaitingForEnclaveTokenForWrapping,
    kWrappingSecrets,
    kJoiningDomain,
  };
  static std::string ToString(State);

  using None = base::StrongAlias<class None, absl::monostate>;
  using Failure =
      base::StrongAlias<class KeyGenerationFailure, absl::monostate>;
  using FileContents = base::StrongAlias<class FileContents, std::string>;
  using KeyReady = base::StrongAlias<
      class KeyGenerated,
      std::pair<std::unique_ptr<crypto::UserVerifyingSigningKey>,
                std::unique_ptr<crypto::UnexportableSigningKey>>>;
  using EnclaveResponse = base::StrongAlias<class EnclaveResponse, cbor::Value>;
  using JoinStatus =
      base::StrongAlias<class JoinStatus,
                        trusted_vault::TrustedVaultRegistrationStatus>;
  using AccessToken = base::StrongAlias<class AccessToken, std::string>;
  using Event = absl::variant<None,
                              Failure,
                              FileContents,
                              KeyReady,
                              EnclaveResponse,
                              AccessToken,
                              JoinStatus>;
  static std::string ToString(const Event&);

  // Moves to `kNextAction` if currently `kIdle`, which will trigger the next
  // requested action.
  void ActIfIdle();

  // The main event loop function, and split out functions to handle each state.
  void Loop(Event);
  void ResetActionState();
  void DoNextAction(Event);
  void StartLoadingState();
  void HandleIdentityChange();
  void StartEnclaveRegistration();
  void DoLoading(Event event);
  void DoGeneratingKeys(Event event);
  void DoWaitingForEnclaveTokenForRegistration(Event event);
  void DoRegisteringWithEnclave(Event event);
  void DoWaitingForEnclaveTokenForWrapping(Event event);
  void DoWrappingSecrets(Event event);
  void JoinDomain();
  void DoJoiningDomain(Event event);

  // Can be called at any point to serialise the current value of `local_state_`
  // to disk. Only a single write happens at a time. If a write is already
  // happening, the request will be queued. If a request is already queued, this
  // call will be ignored.
  void WriteState();
  void DoWriteState();
  void WriteStateComplete(bool success);

  void GenerateHardwareKey(
      std::unique_ptr<crypto::UserVerifyingSigningKey> uv_key);

  void GetAccessTokenInternal();
  static base::flat_map<int32_t, std::vector<uint8_t>> GetNewSecretsToStore(
      const webauthn_pb::EnclaveLocalState_User& user,
      const StoreKeysArgs& args);

  const base::FilePath file_path_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<network::mojom::NetworkContext> network_context_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::unique_ptr<trusted_vault::TrustedVaultConnection>
      trusted_vault_conn_;

  State state_ = State::kInit;
  std::unique_ptr<webauthn_pb::EnclaveLocalState> local_state_;
  raw_ptr<webauthn_pb::EnclaveLocalState_User> user_ = nullptr;
  std::unique_ptr<CoreAccountInfo> primary_account_info_;
  std::unique_ptr<IdentityObserver> identity_observer_;

  bool need_write_ = false;
  bool currently_writing_ = false;
  base::OnceClosure write_finished_callback_;
  std::unique_ptr<StoreKeysArgs> store_keys_args_;

  // These members hold state that only exists for the duration of a sequence of
  // non-idle states. Every time the state machine idles, all these members are
  // reset.
  std::unique_ptr<StoreKeysArgs> store_keys_args_for_joining_;
  std::unique_ptr<crypto::UserVerifyingSigningKey> user_verifying_key_;
  std::unique_ptr<crypto::UserVerifyingKeyProvider>
      user_verifying_key_provider_;
  std::unique_ptr<crypto::UnexportableSigningKey> hardware_key_;
  base::flat_map<int32_t, std::vector<uint8_t>> new_security_domain_secrets_;
  std::unique_ptr<trusted_vault::TrustedVaultConnection::Request> join_request_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  unsigned store_keys_count_ = 0;
  bool want_registration_ = false;
  bool identity_updated_ = true;

  base::ObserverList<Observer> observer_list_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<EnclaveManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_H_
