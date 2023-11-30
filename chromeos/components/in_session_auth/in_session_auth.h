// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_IN_SESSION_AUTH_IN_SESSION_AUTH_H_
#define CHROMEOS_COMPONENTS_IN_SESSION_AUTH_IN_SESSION_AUTH_H_

#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/components/in_session_auth/mojom/in_session_auth.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos::auth {

struct TokenChecker {
  // Check, for user with `account_id`, if the given `token` is valid.
  virtual bool IsTokenValid(const AccountId& account_id,
                            const std::string& token) = 0;
};

// The implementation of the InSessionAuth service.
class InSessionAuth : public chromeos::auth::mojom::InSessionAuth {
 public:
  explicit InSessionAuth();
  ~InSessionAuth() override;
  InSessionAuth(const InSessionAuth&) = delete;
  InSessionAuth& operator=(const InSessionAuth&) = delete;

  void BindReceiver(
      mojo::PendingReceiver<chromeos::auth::mojom::InSessionAuth> receiver);

  // chromeos::auth::mojom::InSessionAuth:
  void RequestToken(chromeos::auth::mojom::Reason reason,
                    const absl::optional<std::string>& prompt,
                    RequestTokenCallback callback) override;
  void CheckToken(chromeos::auth::mojom::Reason reason,
                  const std::string& token,
                  CheckTokenCallback callback) override;
  void InvalidateToken(const std::string& token) override;

 private:
  // Continuation of InSessionAuth::RequestToken. Last 3 params match
  // InSessionAuthDialogController::OnAuthComplete
  void OnAuthComplete(RequestTokenCallback callback,
                      bool success,
                      const ash::AuthProofToken& token,
                      base::TimeDelta timeout);

  mojo::ReceiverSet<chromeos::auth::mojom::InSessionAuth> receivers_;

  base::WeakPtrFactory<InSessionAuth> weak_factory_{this};
};

}  // namespace chromeos::auth

#endif  // CHROMEOS_COMPONENTS_IN_SESSION_AUTH_IN_SESSION_AUTH_H_
