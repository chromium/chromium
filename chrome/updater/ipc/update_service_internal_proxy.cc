// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_internal_proxy.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"

#if BUILDFLAG(IS_POSIX)
#include "chrome/updater/ipc/update_service_internal_proxy_posix.h"
#elif BUILDFLAG(IS_WIN)
#include "chrome/updater/ipc/update_service_internal_proxy_win.h"
#endif

namespace updater {

namespace {

// try_count is the number of attempts made so far.
bool CanRetry(int try_count) {
  return try_count < 3;
}

}  // namespace

UpdateServiceInternalProxy::UpdateServiceInternalProxy(
    scoped_refptr<UpdateServiceInternalProxyImpl> proxy)
    : proxy_(proxy) {}

UpdateServiceInternalProxy::~UpdateServiceInternalProxy() = default;

void UpdateServiceInternalProxy::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_->Run(base::BindOnce(&UpdateServiceInternalProxy::RunDone, this,
                             std::move(callback), 1));
}

void UpdateServiceInternalProxy::Hello(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_->Hello(base::BindOnce(&UpdateServiceInternalProxy::HelloDone, this,
                               std::move(callback), 1));
}

void UpdateServiceInternalProxy::RunDone(base::OnceClosure callback,
                                         int try_count,
                                         std::optional<RpcError> error) {
  if (error && CanRetry(try_count)) {
    proxy_->Run(base::BindOnce(&UpdateServiceInternalProxy::RunDone, this,
                               std::move(callback), try_count + 1));
  } else {
    std::move(callback).Run();
  }
}

void UpdateServiceInternalProxy::HelloDone(base::OnceClosure callback,
                                           int try_count,
                                           std::optional<RpcError> error) {
  if (error && CanRetry(try_count)) {
    proxy_->Hello(base::BindOnce(&UpdateServiceInternalProxy::HelloDone, this,
                                 std::move(callback), try_count + 1));
  } else {
    std::move(callback).Run();
  }
}

}  // namespace updater
