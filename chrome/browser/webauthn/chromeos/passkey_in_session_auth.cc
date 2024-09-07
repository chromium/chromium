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
#include "chromeos/ash/components/osauth/impl/request/webauthn_auth_request.h"
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
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {
namespace {

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
    if (ash::features::IsWebAuthNAuthDialogMergeEnabled()) {
      auto webauthn_auth_request = std::make_unique<ash::WebAuthNAuthRequest>(
          rp_id, std::move(result_callback));
      ash::ActiveSessionAuthController::Get()->ShowAuthDialog(
          std::move(webauthn_auth_request));
      return;
    }

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
            static_cast<int>(
                chromeos::auth::mojom::InSessionAuth::MethodMinVersions::
                    kRequestLegacyWebAuthnMinVersion)) {
      FIDO_LOG(ERROR)
          << "Failed to perform UV because InSessionAuth is not available";
      std::move(result_callback).Run(false);
      return;
    }
    auto* host = views::DesktopWindowTreeHostLacros::From(window->GetHost());
    if (!host) {
      FIDO_LOG(ERROR)
          << "Failed to perform UV because window host can't be found";
      std::move(result_callback).Run(false);
      return;
    }
    auto* platform_window = host->platform_window();
    if (!platform_window) {
      FIDO_LOG(ERROR)
          << "Failed to perform UV because platform window can't be found";
      std::move(result_callback).Run(false);
      return;
    }
    lacros_service->GetRemote<chromeos::auth::mojom::InSessionAuth>()
        ->RequestLegacyWebAuthn(rp_id, platform_window->GetWindowUniqueId(),
                                std::move(result_callback));
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
