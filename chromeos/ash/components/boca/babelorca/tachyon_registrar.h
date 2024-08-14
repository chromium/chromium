// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_REGISTRAR_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_REGISTRAR_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/babelorca/response_callback_wrapper.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash::babelorca {

class SignInGaiaResponse;
class TachyonAuthedClient;

// Register user with Tachyon and store tachyon token to be used by other
// tachyon requests.
class TachyonRegistrar {
 public:
  TachyonRegistrar(
      TachyonAuthedClient* authed_client,
      const net::NetworkTrafficAnnotationTag& network_annotation_tag);

  TachyonRegistrar(const TachyonRegistrar&) = delete;
  TachyonRegistrar& operator=(const TachyonRegistrar&) = delete;

  ~TachyonRegistrar();

  void Register(const std::string& client_uuid,
                base::OnceCallback<void(bool)> success_cb);

  // Tachyon token fetched from registration response. `nullopt` if registration
  // did not start or still in progress, of if registration request failed.
  std::optional<std::string> GetTachyonToken();

 private:
  void OnResponse(
      base::OnceCallback<void(bool)> success_cb,
      base::expected<SignInGaiaResponse,
                     ResponseCallbackWrapper::TachyonRequestError> response);

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<TachyonAuthedClient> authed_client_;
  const raw_ref<const net::NetworkTrafficAnnotationTag> network_annotation_tag_;
  std::optional<std::string> tachyon_token_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<TachyonRegistrar> weak_ptr_factory{this};
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_REGISTRAR_H_
