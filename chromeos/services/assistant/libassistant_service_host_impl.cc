// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/libassistant_service_host_impl.h"

#include "base/check.h"
#include "base/synchronization/lock.h"
#include "chromeos/services/libassistant/libassistant_service.h"

namespace chromeos {
namespace assistant {

LibassistantServiceHostImpl::LibassistantServiceHostImpl(
    CrosPlatformApi* platform_api,
    AssistantManagerServiceDelegate* delegate)
    : platform_api_(platform_api), delegate_(delegate) {
  DCHECK(platform_api_);
  DCHECK(delegate_);
}

LibassistantServiceHostImpl::~LibassistantServiceHostImpl() = default;

void LibassistantServiceHostImpl::Launch(
    mojo::PendingReceiver<LibassistantServiceMojom> receiver) {
  base::AutoLock lock(libassistant_service_lock_);
  DCHECK(!libassistant_service_);
  libassistant_service_ =
      std::make_unique<chromeos::libassistant::LibassistantService>(
          std::move(receiver), platform_api_, delegate_);

  if (pending_initialize_callback_) {
    libassistant_service_->SetInitializeCallback(
        std::move(pending_initialize_callback_));
  }
}

void LibassistantServiceHostImpl::Stop() {
  base::AutoLock lock(libassistant_service_lock_);
  libassistant_service_ = nullptr;
}

void LibassistantServiceHostImpl::SetInitializeCallback(
    InitializeCallback callback) {
  base::AutoLock lock(libassistant_service_lock_);

  if (libassistant_service_) {
    libassistant_service_->SetInitializeCallback(std::move(callback));
  } else {
    // Launch() is called on the background thread and SetInitializeCallback()
    // on the main thread, so it is possible we come here before Launch() has
    // had a chance to run. If that happens we remember the callback and pass
    // it to the service in Launch().
    pending_initialize_callback_ = std::move(callback);
  }
}

}  // namespace assistant
}  // namespace chromeos
