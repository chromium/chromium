// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chromeos/passkey_in_session_auth.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "ui/aura/window.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "ash/public/cpp/webauthn_dialog_controller.h"
#include "ash/shell.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/components/in_session_auth/mojom/in_session_auth.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {
namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void OnRequestToken(base::OnceCallback<void(bool)> callback,
                    chromeos::auth::mojom::RequestTokenReplyPtr reply) {
  std::move(callback).Run(/*success=*/!reply.is_null());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

PasskeyInSessionAuthProvider* g_instance = nullptr;
PasskeyInSessionAuthProvider* g_override = nullptr;

class PasskeyInSessionAuthProviderImpl : public PasskeyInSessionAuthProvider {
 public:
  PasskeyInSessionAuthProviderImpl() = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void ShowPasskeyInSessionAuthDialog(
      aura::Window* window,
      const std::string& rp_id,
      base::OnceCallback<void(bool)> result_callback) override {
    ash::Shell::Get()->webauthn_dialog_controller()->ShowAuthenticationDialog(
        window, rp_id, std::move(result_callback));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void ShowPasskeyInSessionAuthDialog(
      aura::Window* window,
      const std::string& rp_id,
      base::OnceCallback<void(bool)> result_callback) override {
    auto* lacros_service = chromeos::LacrosService::Get();
    if (!lacros_service->IsAvailable<chromeos::auth::mojom::InSessionAuth>() ||
        lacros_service
                ->GetInterfaceVersion<chromeos::auth::mojom::InSessionAuth>() <
            static_cast<int>(chromeos::auth::mojom::InSessionAuth::
                                 MethodMinVersions::kRequestTokenMinVersion)) {
      FIDO_LOG(ERROR)
          << "Failed to perform UV because InSessionAuth is not available";
      std::move(result_callback).Run(false);
      return;
    }
    // TODO(crbug.com/40187814): Implement a passkeys-specific in-session auth
    // reason.
    lacros_service->GetRemote<chromeos::auth::mojom::InSessionAuth>()
        ->RequestToken(
            chromeos::auth::mojom::Reason::kAccessPasswordManager, std::nullopt,
            base::BindOnce(&OnRequestToken, std::move(result_callback)));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

}  // namespace

PasskeyInSessionAuthProvider::~PasskeyInSessionAuthProvider() = default;

PasskeyInSessionAuthProvider* PasskeyInSessionAuthProvider::Get() {
  if (g_override) {
    return g_override;
  }
  if (!g_instance) {
    g_instance = new PasskeyInSessionAuthProviderImpl();
  }
  return g_instance;
}

void PasskeyInSessionAuthProvider::SetInstanceForTesting(
    PasskeyInSessionAuthProvider* test_override) {
  CHECK(!g_override || !test_override)
      << "Cannot override PasskeyInSessionAuthProvider twice.";
  g_override = test_override;
}

}  // namespace chromeos
