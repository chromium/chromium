// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_KCER_KCER_NSS_TEST_UTILS_H_
#define CHROMEOS_COMPONENTS_KCER_KCER_NSS_TEST_UTILS_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/kcer/kcer.h"
#include "chromeos/components/kcer/kcer_nss/kcer_token_impl_nss.h"
#include "chromeos/components/kcer/key_permissions.pb.h"
#include "crypto/scoped_test_nss_db.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace kcer {

// A helper class to work with tokens (that exist on the IO thread) from the UI
// thread.
class TokenHolder {
 public:
  // Creates a KcerToken of the type `token` and moves it to the IO thread. If
  // `initialize` then the KcerToken will be ready to process requests
  // immediately.
  explicit TokenHolder(Token token, bool initialize);
  ~TokenHolder();

  // If KcerToken was not initialized on construction, this method makes it
  // initialized. Can be used to simulate delayed initialization.
  void Initialize();
  // If KcerToken was not initialized on construction, this method simulates
  // initialization failure.
  void FailInitialization();

  // Returns a weak pointer to the token that can be used to post requests for
  // it. The pointer should only be dereferenced on the IO thread.
  base::WeakPtr<internal::KcerTokenImplNss> GetWeakPtr() { return weak_ptr_; }

 private:
  base::WeakPtr<internal::KcerTokenImplNss> weak_ptr_;
  std::unique_ptr<internal::KcerTokenImplNss> io_token_;
  crypto::ScopedTestNSSDB nss_slot_;
  bool is_initialized_ = false;
};

// Compares two KerPermissions, returns true if they are equal.
bool KeyPermissionsEqual(const absl::optional<chaps::KeyPermissions>& a,
                         const absl::optional<chaps::KeyPermissions>& b);

// Verifies `signature` created with `signing_scheme` and the public key from
// `spki` for `data_to_sign`. By default (with `strict` == true) only returns
// true if the signature is correct. With `strict` == false, silently ignores
// schemes for which the verification is not implemented yet and also returns
// true for them. Returns false if signature is incorrect.
bool VerifySignature(SigningScheme signing_scheme,
                     PublicKeySpki spki,
                     DataToSign data_to_sign,
                     Signature signature,
                     bool strict = true);

// Returns |hash| prefixed with DER-encoded PKCS#1 DigestInfo with
// AlgorithmIdentifier=id-sha256.
// This is useful for testing Kcer::SignRsaPkcs1Raw which only
// appends PKCS#1 v1.5 padding before signing.
std::vector<uint8_t> PrependSHA256DigestInfo(base::span<const uint8_t> hash);

}  // namespace kcer

#endif  // CHROMEOS_COMPONENTS_KCER_KCER_NSS_TEST_UTILS_H_
