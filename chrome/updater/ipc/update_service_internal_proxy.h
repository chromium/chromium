// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/updater/update_service_internal.h"

#if BUILDFLAG(IS_POSIX)
#include "chrome/updater/ipc/update_service_internal_proxy_posix.h"
#elif BUILDFLAG(IS_WIN)
#include "chrome/updater/ipc/update_service_internal_proxy_win.h"
#endif

namespace updater {

class UpdateServiceInternalProxy : public UpdateServiceInternal {
 public:
  explicit UpdateServiceInternalProxy(
      scoped_refptr<UpdateServiceInternalProxyImpl> proxy);

  // Overrides for UpdateServiceInternal.
  // UpdateServiceInternalProxy will not be destroyed while these calls
  // are outstanding; the caller need not retain a ref.
  void Run(base::OnceClosure callback) override;
  void Hello(base::OnceClosure callback) override;

 private:
  ~UpdateServiceInternalProxy() override;

  void RunDone(base::OnceClosure callback,
               int try_count,
               std::optional<RpcError> error);
  void HelloDone(base::OnceClosure callback,
                 int try_count,
                 std::optional<RpcError> result);

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdateServiceInternalProxyImpl> proxy_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_H_
