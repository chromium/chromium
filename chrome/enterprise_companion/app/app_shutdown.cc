// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/enterprise_companion/app/app.h"
#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"

namespace enterprise_companion {

// AppShutdown sends an IPC to the running EnterpriseCompanion instructing it to
// shutdown, if present.
class AppShutdown : public App {
 public:
  explicit AppShutdown(
      const mojo::NamedPlatformChannel::ServerName& server_name)
      : server_name_(server_name) {}

  ~AppShutdown() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  void FirstTaskRun() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    ConnectToServer(base::BindOnce(&AppShutdown::OnConnected,
                                   weak_ptr_factory_.GetWeakPtr()),
                    server_name_);
  }

  void OnConnected(std::unique_ptr<mojo::IsolatedConnection> connection,
                   mojo::Remote<mojom::EnterpriseCompanion> remote) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!connection || !remote) {
      Shutdown(EnterpriseCompanionStatus(
          ApplicationError::kEnterpriseCompanionServiceConnectionFailed));
      return;
    }

    connection_ = std::move(connection);
    remote_ = std::move(remote);

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

    Shutdown(EnterpriseCompanionStatus(
        ApplicationError::kEnterpriseCompanionServiceConnectionFailed));
  }

  SEQUENCE_CHECKER(sequence_checker_);
  const mojo::NamedPlatformChannel::ServerName server_name_;
  std::unique_ptr<mojo::IsolatedConnection> connection_;
  mojo::Remote<mojom::EnterpriseCompanion> remote_;
  base::WeakPtrFactory<AppShutdown> weak_ptr_factory_{this};
};

std::unique_ptr<App> CreateAppShutdown(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  return std::make_unique<AppShutdown>(server_name);
}

}  // namespace enterprise_companion
