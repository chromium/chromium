// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/libassistant_service_host_impl.h"

#include "base/check.h"
#include "chromeos/services/libassistant/libassistant_service.h"

namespace chromeos {
namespace assistant {

LibassistantServiceHostImpl::LibassistantServiceHostImpl(
    assistant_client::PlatformApi* platform_api,
    AssistantManagerServiceDelegate* delegate)
    : platform_api_(platform_api), delegate_(delegate) {
  DCHECK(platform_api_);
  DCHECK(delegate_);
}

LibassistantServiceHostImpl::~LibassistantServiceHostImpl() = default;

void LibassistantServiceHostImpl::Launch(
    mojo::PendingReceiver<LibassistantServiceMojom> receiver) {
  DCHECK_EQ(libassistant_service_, nullptr);
  libassistant_service_ =
      std::make_unique<chromeos::libassistant::LibassistantService>(
          std::move(receiver), platform_api_, delegate_);
}

void LibassistantServiceHostImpl::Stop() {
  libassistant_service_ = nullptr;
}

void LibassistantServiceHostImpl::SetInitializeCallback(
    base::OnceCallback<void(assistant_client::AssistantManager*,
                            assistant_client::AssistantManagerInternal*)>
        callback) {
  DCHECK_NE(libassistant_service_, nullptr);
  libassistant_service_->SetInitializeCallback(std::move(callback));
}

}  // namespace assistant
}  // namespace chromeos
