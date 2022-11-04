// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LEGACY_FINGERPRINT_ENGINE_H_
#define CHROME_BROWSER_UI_ASH_LEGACY_FINGERPRINT_ENGINE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AccountId;

namespace ash {

class UserContext;
class AuthPerformer;

// Handles Fingerprint authentication operations and is the source of truth for
// the availability of Fingerprint authentication.
class LegacyFingerprintEngine {
 public:
  enum class Purpose { kAny, kUnlock, kWebAuthn };

  explicit LegacyFingerprintEngine(ash::AuthPerformer* auth_performer);
  LegacyFingerprintEngine(const LegacyFingerprintEngine&) = delete;
  LegacyFingerprintEngine& operator=(const LegacyFingerprintEngine&) = delete;
  virtual ~LegacyFingerprintEngine();

  bool IsFingerprintAvailable(Purpose purpose, const AccountId& account_id);

  void PrepareLegacyFingerprintFactor(std::unique_ptr<UserContext> user_context,
                                      AuthOperationCallback callback);
  void TerminateLegacyFingerprintFactor(
      std::unique_ptr<UserContext> user_context,
      AuthOperationCallback callback);

 private:
  const base::raw_ptr<AuthPerformer> auth_performer_;

  base::WeakPtrFactory<LegacyFingerprintEngine> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LEGACY_FINGERPRINT_ENGINE_H_
