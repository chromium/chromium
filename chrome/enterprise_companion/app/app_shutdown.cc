// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/enterprise_companion/app/app.h"
#include "chrome/enterprise_companion/app/app_client_base.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace enterprise_companion {

// AppShutdown sends an IPC to the running EnterpriseCompanion instructing it to
// shutdown, if present.
class AppShutdown : public AppClientBase {
 public:
  explicit AppShutdown(
      const mojo::NamedPlatformChannel::ServerName& server_name)
      : AppClientBase(server_name) {}

  ~AppShutdown() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  void OnRemoteReady() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    remote_->Shutdown(mojo::WrapCallbackWithDropHandler(
        base::BindOnce(&AppShutdown::OnRemoteShutdown,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&AppShutdown::OnRPCDropped,
                       weak_ptr_factory_.GetWeakPtr())));
  }

  void OnRemoteShutdown(mojom::StatusPtr status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    Shutdown(EnterpriseCompanionStatus::FromMojomStatus(std::move(status)));
  }

  void OnRPCDropped() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    Shutdown(
        EnterpriseCompanionStatus(ApplicationError::kMojoConnectionFailed));
  }

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AppShutdown> weak_ptr_factory_{this};
};

std::unique_ptr<App> CreateAppShutdown(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  return std::make_unique<AppShutdown>(server_name);
}

}  // namespace enterprise_companion
