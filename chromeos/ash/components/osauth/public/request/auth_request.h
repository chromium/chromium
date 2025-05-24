// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_REQUEST_AUTH_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_REQUEST_AUTH_REQUEST_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"

namespace ash {

class UserContext;

// This class encapsulates logic that governs behaviors specific to
// a given `ActiveSessionAuthController::Reason`.
// `AuthRequest` is passed to `ActiveSessionAuthController::ShowAuthDialog`
// to modify various behaviors of the authentication.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthRequest {
 public:
  // The enums below are used in histograms, do not remove/renumber entries. If
  // you're adding to any of these enums, update
  // ActiveSessionAuthControllerReason in
  // tools/metrics/histograms/metadata/ash/enums.xml:
  enum class Reason {
    kPasswordManager = 0,
    kSettings = 1,
    kWebAuthN = 2,
    kMaxValue = kWebAuthN
  };

  // Returns the AuthSession intent to be used for the AuthRequest.
  // This differs from `GetAuthReason` in that in returns the
  // cryptohome-level intent of the authentication. i.e kDecrypt vs
  // kVerifyOnly vs kWebAuthN.
  virtual AuthSessionIntent GetAuthSessionIntent() const = 0;

  // Return the reason from authentication. This is the client that
  // requested the authentication. i.e ChromeOS settings, Chrome
  // password manager, WebAuthN.
  virtual Reason GetAuthReason() const = 0;

  // Returns a description to attach under the dialog's Titlebar,
  // describing the purpose of the authentication. i.e "ChromeOS
  // settings would like to know it's you".
  virtual const std::u16string GetDescription() const = 0;

  // Notified clients of the authentication of success or failure.
  virtual void NotifyAuthSuccess(std::unique_ptr<UserContext> user_context) = 0;
  virtual void NotifyAuthFailure() = 0;

  virtual ~AuthRequest() = default;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_REQUEST_AUTH_REQUEST_H_
