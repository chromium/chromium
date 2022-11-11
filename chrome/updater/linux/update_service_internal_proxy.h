// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_LINUX_UPDATE_SERVICE_INTERNAL_PROXY_H_
#define CHROME_UPDATER_LINUX_UPDATE_SERVICE_INTERNAL_PROXY_H_

#include "chrome/updater/app/server/linux/mojom/updater_service_internal.mojom-forward.h"
#include "chrome/updater/update_service_internal.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace mojo {
class IsolatedConnection;
}  // namespace mojo

namespace updater {

enum class UpdaterScope;
class UpdateServiceInternalProxyImpl;

class UpdateServiceInternalProxy : public UpdateServiceInternal {
 public:
  UpdateServiceInternalProxy(
      UpdaterScope scope,
      std::unique_ptr<mojo::IsolatedConnection> connection,
      mojo::Remote<mojom::UpdateServiceInternal> remote);

  // Overrides for UpdateServiceInternal.
  void Run(base::OnceClosure callback) override;
  void Hello(base::OnceClosure callback) override;

 private:
  ~UpdateServiceInternalProxy() override;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdateServiceInternalProxyImpl> impl_;
};

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxy(
    UpdaterScope scope,
    std::unique_ptr<mojo::IsolatedConnection> connection,
    mojo::Remote<mojom::UpdateServiceInternal> remote);

}  // namespace updater

#endif  // CHROME_UPDATER_LINUX_UPDATE_SERVICE_INTERNAL_PROXY_H_
