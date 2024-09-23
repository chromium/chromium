// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_SESSION_STORAGE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_SESSION_STORAGE_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace base {

class Location;

}  // namespace base

namespace ash {

class UserContext;

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) ScopedSessionRefresher {
 public:
  ScopedSessionRefresher(const ScopedSessionRefresher&) = delete;
  ScopedSessionRefresher& operator=(const ScopedSessionRefresher&) = delete;
  virtual ~ScopedSessionRefresher() = default;

 protected:
  ScopedSessionRefresher() = default;
};

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
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthSessionStorage {
 public:
  using InvalidationCallback = base::OnceCallback<void(void)>;

  // TODO (b/271249180): Define an observer for notifications about token
  // expiration/borrowing.

  // Convenience method.
  static inline AuthSessionStorage* Get() {
    return AuthParts::Get()->GetAuthSessionStorage();
  }

  virtual ~AuthSessionStorage() = default;

  // Gets the ownership of (authenticated) context, and returns an
  // authentication proof token that can be safely passed around between
  // components. Storage manages the lifetime of the context (and
  // underlying cyrptohome authsession).
  virtual AuthProofToken Store(std::unique_ptr<UserContext> context) = 0;
  // Checks if given token is valid (exists and has not expired).
  virtual bool IsValid(const AuthProofToken& token) = 0;

  virtual std::unique_ptr<UserContext> BorrowForTests(
      const base::Location& location,
      const AuthProofToken& token) = 0;

  // Borrows UserContext to perform some authenticated operation. Borrowing
  // a context does not make it invalid.
  // If context is borrowed at the moment of the call, the callback
  // would be called once the context is returned to the storage.
  // Note that callback might be called with `null` value, if
  // the context would become invalid before it is returned.
  virtual void BorrowAsync(const base::Location& location,
                           const AuthProofToken& token,
                           BorrowContextCallback callback) = 0;

  // Allows client to obtain UserContext without intent to return it back.
  // Takes precedence over Borrow requests, but not over invalidate
  // request.
  // Withdrawing context from the storage makes associated token invalid.
  //
  // If context is borrowed at the moment of the call, the callback
  // would be called once the context is returned to the storage.
  // Note that callback might be called with `null` value, if
  // the context would become invalid before it is returned.
  //
  // There can be only one Withdraw request at one time, requesting parallel
  // Withdraw request would result in crash.
  virtual void Withdraw(const AuthProofToken& token,
                        BorrowContextCallback callback) = 0;

  // Allows to inspect stored UserContext. The reference is only valid within
  // same UI event, and should not be stored by caller.
  virtual const UserContext* Peek(const AuthProofToken& token) = 0;
  // Return context back to Storage.
  virtual void Return(const AuthProofToken& token,
                      std::unique_ptr<UserContext> context) = 0;

  // Cleans up UserContext and all associated resources (like cryptohome
  // AuthSession) once authentication is no longer needed.
  // In case when context is borrowed at the time of this call,
  // it would be properly invalidated once it is returned.
  virtual void Invalidate(
      const AuthProofToken& token,
      std::optional<InvalidationCallback> on_invalidated) = 0;

  // This method allows caller to make sure that authenticated authsession
  // associated with `token` would not expire by timeout as long as returned
  // object exists. AuthSessionStorage would automatically issue refresh calls
  // at required intervals. Note that this would only happen while Storage
  // actually has a UserContext. If it was borrowed, it's lifetime would be
  // refreshed only upon returning.
  virtual std::unique_ptr<ScopedSessionRefresher> KeepAlive(
      const AuthProofToken& token) = 0;

  // Checks whether there is a keep alive for the given token. Used by tests
  // that verify the usage of ScopedSessionRefresher on certain screens.
  virtual bool CheckHasKeepAliveForTesting(
      const AuthProofToken& token) const = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_SESSION_STORAGE_H_
