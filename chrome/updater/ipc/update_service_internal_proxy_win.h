// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_WIN_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_WIN_H_

#include <windows.h>

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/ipc/update_service_internal_proxy_impl.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

enum class UpdaterScope;
class UpdateServiceInternalProxyImplImpl;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceInternalProxyWinImpl
    : public UpdateServiceInternalProxyImpl {
 public:
  explicit UpdateServiceInternalProxyWinImpl(UpdaterScope scope);

  void Run(base::OnceCallback<void(std::optional<RpcError>)> callback) override;
  void Hello(
      base::OnceCallback<void(std::optional<RpcError>)> callback) override;

 private:
  ~UpdateServiceInternalProxyWinImpl() override;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdateServiceInternalProxyImplImpl> impl_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_WIN_H_
