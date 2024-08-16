// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_APP_APP_CLIENT_BASE_H_
#define CHROME_ENTERPRISE_COMPANION_APP_APP_CLIENT_BASE_H_

#include <memory>

#include "base/sequence_checker.h"
#include "chrome/enterprise_companion/app/app.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace mojo {
class IsolatedConnection;
}

namespace enterprise_companion {

// Base class for applications which are IPC clients of the companion app.
class AppClientBase : public App {
 public:
  explicit AppClientBase(
      const mojo::NamedPlatformChannel::ServerName& server_name);
  ~AppClientBase() override;

 protected:
  mojo::Remote<mojom::EnterpriseCompanion> remote_;

  // Called on the constructing sequence when `remote_` is ready to dispatch
  // calls. If a connection could not be established, `Shutdown` will be called
  // and this method will not be invoked.
  virtual void OnRemoteReady() = 0;

 private:
  void OnConnected(std::unique_ptr<mojo::IsolatedConnection> connection,
                   mojo::Remote<mojom::EnterpriseCompanion> remote);
  // Overrides for App.
  void FirstTaskRun() override;

  SEQUENCE_CHECKER(sequence_checker_);
  const mojo::NamedPlatformChannel::ServerName server_name_;
  std::unique_ptr<mojo::IsolatedConnection> connection_;
  base::WeakPtrFactory<AppClientBase> weak_ptr_factory_{this};
};

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_APP_APP_CLIENT_BASE_H_
