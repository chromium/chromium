// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_SESSION_STORAGE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_SESSION_STORAGE_IMPL_H_

#include <memory>
#include <queue>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class AuthenticationError;
class AuthPerformer;
class UserContext;
class UserDataAuthClient;

// Helper class that stores and manages lifetime of authenticated UserContext.
// Main use cases for this class are the situations where authenticated
// operations do not happen immediately after authentication, but require some
// user input, e.g. setting up additional factors during user onboarding on a
// first run, or entering authentication-related section of
// `chrome://os-settings`.
//
// When context is added to storage, storage would return a token as a
// replacement, this token can be relatively safely be passed between components
// as it does not contain any sensitive information.
//
// UserContext can be borrowed to perform authenticated operations and should be
// returned to storage as soon as operation completes.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthSessionStorageImpl
    : public AuthSessionStorage {
 public:
  explicit AuthSessionStorageImpl(UserDataAuthClient* user_data_auth);
  ~AuthSessionStorageImpl() override;

  // AuthSessionStorage implementation:
  AuthProofToken Store(std::unique_ptr<UserContext> context) override;
  bool IsValid(const AuthProofToken& token) override;
  std::unique_ptr<UserContext> BorrowForTests(
      const base::Location& location,
      const AuthProofToken& token) override;
  void BorrowAsync(const base::Location& location,
                   const AuthProofToken& token,
                   BorrowCallback callback) override;
  const UserContext* Peek(const AuthProofToken& token) override;
  void Return(const AuthProofToken& token,
              std::unique_ptr<UserContext> context) override;
  void Invalidate(const AuthProofToken& token,
                  base::OnceClosure on_invalidated) override;

 private:
  enum class TokenState {
    kOwned,         // UserContext is owned by storage
    kBorrowed,      // UserContext is currently borrowed
    kInvalidating,  // token is being invalidated
  };

  struct TokenData {
    explicit TokenData(std::unique_ptr<UserContext> context);
    ~TokenData();

    // Context associated with token
    std::unique_ptr<UserContext> context;
    TokenState state = TokenState::kOwned;

    // Code location of the last borrow operation.
    base::Location borrow_location;

    // Data required to invalidate context upon return, if invalidation was
    // requested while context is borrowed.
    bool invalidate_on_return = false;
    base::OnceClosure invalidation_closure;
    std::queue<std::pair<base::Location, BorrowCallback>> borrow_queue;
  };

  std::unique_ptr<UserContext> Borrow(const base::Location& location,
                                      const AuthProofToken& token);
  void OnSessionInvalidated(const AuthProofToken& token,
                            base::OnceClosure on_invalidated,
                            std::unique_ptr<UserContext> context,
                            absl::optional<AuthenticationError> error);

  // Stored data for currently active tokens.
  base::flat_map<AuthProofToken, std::unique_ptr<TokenData>> tokens_;

  std::unique_ptr<AuthPerformer> auth_performer_;

  base::WeakPtrFactory<AuthSessionStorageImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_SESSION_STORAGE_IMPL_H_
