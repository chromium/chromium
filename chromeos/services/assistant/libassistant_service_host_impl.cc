// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/libassistant_service_host_impl.h"

#include "base/check.h"
#include "base/sequence_checker.h"
#include "build/buildflag.h"
#include "chromeos/assistant/buildflags.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/services/libassistant/libassistant_service.h"

#if BUILDFLAG(ENABLE_LIBASSISTANT_SANDBOX)
#include "chromeos/services/assistant/public/cpp/assistant_client.h"  // nogncheck
#include "chromeos/services/libassistant/public/mojom/service.mojom-forward.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_LIBASSISTANT_SANDBOX)
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)

namespace chromeos {
namespace assistant {

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)

LibassistantServiceHostImpl::LibassistantServiceHostImpl() {
#if !BUILDFLAG(ENABLE_LIBASSISTANT_SANDBOX)
  DETACH_FROM_SEQUENCE(sequence_checker_);
#endif
}

LibassistantServiceHostImpl::~LibassistantServiceHostImpl() = default;

void LibassistantServiceHostImpl::Launch(
    mojo::PendingReceiver<chromeos::libassistant::mojom::LibassistantService>
        receiver) {
#if BUILDFLAG(ENABLE_LIBASSISTANT_SANDBOX)
  AssistantClient::Get()->RequestLibassistantService(std::move(receiver));
#else
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!libassistant_service_);
  libassistant_service_ =
      std::make_unique<chromeos::libassistant::LibassistantService>(
          std::move(receiver));
#endif  // BUILDFLAG(ENABLE_LIBASSISTANT_SANDBOX)
}

void LibassistantServiceHostImpl::Stop() {
#if !BUILDFLAG(ENABLE_LIBASSISTANT_SANDBOX)
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  libassistant_service_ = nullptr;
#endif
}

#else

LibassistantServiceHostImpl::LibassistantServiceHostImpl() = default;
LibassistantServiceHostImpl::~LibassistantServiceHostImpl() = default;

void LibassistantServiceHostImpl::Launch(
    mojo::PendingReceiver<chromeos::libassistant::mojom::LibassistantService>
        receiver) {}

void LibassistantServiceHostImpl::Stop() {}

#endif

}  // namespace assistant
}  // namespace chromeos
