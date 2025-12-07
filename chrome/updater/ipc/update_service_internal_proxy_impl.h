// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_IMPL_H_
#define CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_IMPL_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

class UpdateServiceInternalProxyImpl
    : public base::RefCountedThreadSafe<UpdateServiceInternalProxyImpl> {
 public:
  virtual void Run(
      base::OnceCallback<void(std::optional<RpcError>)> callback) = 0;
  virtual void Hello(
      base::OnceCallback<void(std::optional<RpcError>)> callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<UpdateServiceInternalProxyImpl>;

  virtual ~UpdateServiceInternalProxyImpl() = default;
};

}  // namespace updater

#endif  // CHROME_UPDATER_IPC_UPDATE_SERVICE_INTERNAL_PROXY_IMPL_H_
