// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/libassistant_service_host_impl.h"

#include "base/check.h"
#include "base/sequence_checker.h"
#include "chromeos/services/libassistant/libassistant_service.h"

namespace chromeos {
namespace assistant {

LibassistantServiceHostImpl::LibassistantServiceHostImpl(
    AssistantManagerServiceDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

LibassistantServiceHostImpl::~LibassistantServiceHostImpl() = default;

void LibassistantServiceHostImpl::Launch(
    mojo::PendingReceiver<chromeos::libassistant::mojom::LibassistantService>
        receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!libassistant_service_);
  libassistant_service_ =
      std::make_unique<chromeos::libassistant::LibassistantService>(
          std::move(receiver), delegate_);
}

void LibassistantServiceHostImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  libassistant_service_ = nullptr;
}

}  // namespace assistant
}  // namespace chromeos
