// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/app/app_client_base.h"

#include "base/sequence_checker.h"
#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"

namespace enterprise_companion {

AppClientBase::AppClientBase(
    const mojo::NamedPlatformChannel::ServerName& server_name)
    : server_name_(server_name) {}

AppClientBase::~AppClientBase() = default;

void AppClientBase::OnConnected(
    std::unique_ptr<mojo::IsolatedConnection> connection,
    mojo::Remote<mojom::EnterpriseCompanion> remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!connection || !remote) {
    Shutdown(
        EnterpriseCompanionStatus(ApplicationError::kMojoConnectionFailed));
    return;
  }

  connection_ = std::move(connection);
  remote_ = std::move(remote);
  OnRemoteReady();
}

void AppClientBase::FirstTaskRun() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ConnectToServer(base::BindOnce(&AppClientBase::OnConnected,
                                 weak_ptr_factory_.GetWeakPtr()),
                  server_name_);
}

}  // namespace enterprise_companion
