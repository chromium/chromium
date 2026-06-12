// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_BINDING_KEY_REGISTRATION_TOKEN_HELPER_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_BINDING_KEY_REGISTRATION_TOKEN_HELPER_H_

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/signin/public/base/binding_key_registration_token_result.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"
#include "url/gurl.h"

namespace base {
class Time;
}

namespace unexportable_keys {
class UnexportableKeyService;
class UnexportableKeyLoader;
}  // namespace unexportable_keys

namespace signin {

// Helper class for generating registration tokens to bind the key on the
// server.
//
// A single instance can be used to generate multiple registration tokens for
// the same binding key. To use different binding keys, create multiple class
// instances.
//
// TODO(alexilin): support a timeout aborting the token generation if it takes
// too long.
//
// TODO(crbug.com/516196445): move this class into
// `//components/signin/internal/identity_manager/` once it's no longer used
// outside of the signin component.
class BindingKeyRegistrationTokenHelper {
 public:
  // Initialization parameter indicating which binding key should be used for
  // registration token generation.
  using KeyInitParam = std::variant<
      // A list of acceptable signature algorithms to generate a new binding
      // key.
      std::vector<crypto::SignatureVerifier::SignatureAlgorithm>,
      // Wrapped binding key to reuse an existing binding key.
      std::vector<uint8_t>>;

  using Result = BindingKeyRegistrationTokenResult;

  // Public for testing.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(Error)
  enum class Error {
    kNone = 0,
    kLoadReusedKeyFailure = 1,
    kGenerateNewKeyFailure = 2,
    kCreateAssertionFailure = 3,
    kSignAssertionFailure = 4,
    kAppendSignatureFailure = 5,
    kMaxValue = kAppendSignatureFailure
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:BoundSessionCredentialsRegistrationTokenResult)

  // `unexportable_key_service` must outlive `this`.
  BindingKeyRegistrationTokenHelper(
      unexportable_keys::UnexportableKeyService& unexportable_key_service,
      KeyInitParam key_init_param);

  BindingKeyRegistrationTokenHelper(const BindingKeyRegistrationTokenHelper&) =
      delete;
  BindingKeyRegistrationTokenHelper& operator=(
      const BindingKeyRegistrationTokenHelper&) = delete;

  virtual ~BindingKeyRegistrationTokenHelper();

  // Initiates loading or generation of the binding key if not already started.
  void CreateKeyLoaderIfNeeded();

  // Returns `true` if the binding key was successfully generated or unwrapped.
  // Returns `false` if the key hasn't been created yet or if it failed to
  // create/unwrap.
  bool IsRegistrationKeyReady() const;

  // Invokes `callback` with a `Result` containing a new binding key ID and a
  // corresponding registration token on success. Otherwise, invokes `callback`
  // with `std::nullopt`.
  // Virtual for testing.
  virtual void GenerateForSessionBinding(
      std::string_view challenge,
      const GURL& registration_url,
      base::OnceCallback<void(std::optional<Result>)> callback);
  virtual void GenerateForTokenBinding(
      std::string_view client_id,
      const std::variant<TokenBindingAuthCode, TokenBindingChallenge>&
          auth_code_or_challenge,
      const GURL& registration_url,
      base::OnceCallback<void(std::optional<Result>)> callback);

 private:
  using HeaderAndPayloadGenerator =
      base::RepeatingCallback<std::optional<std::string>(
          crypto::SignatureVerifier::SignatureAlgorithm,
          base::span<const uint8_t>,
          base::Time)>;

  void SignHeaderAndPayload(
      HeaderAndPayloadGenerator header_and_payload_generator,
      base::OnceCallback<void(base::expected<Result, Error>)> callback,
      unexportable_keys::ServiceErrorOr<
          unexportable_keys::UnexportableSigningKeyId> binding_key);
  void CreateRegistrationToken(
      std::string_view header_and_payload,
      unexportable_keys::UnexportableKeyId binding_key,
      base::OnceCallback<void(base::expected<Result, Error>)> callback,
      unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> signature);
  static void RecordResultAndInvokeCallback(
      std::string_view result_histogram_name,
      base::OnceCallback<void(std::optional<Result>)> callback,
      base::expected<Result, Error> result_or_error);

  const raw_ref<unexportable_keys::UnexportableKeyService>
      unexportable_key_service_;
  const KeyInitParam key_init_param_;

  std::unique_ptr<unexportable_keys::UnexportableKeyLoader> key_loader_;
  base::WeakPtrFactory<BindingKeyRegistrationTokenHelper> weak_ptr_factory_{
      this};
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_BINDING_KEY_REGISTRATION_TOKEN_HELPER_H_
