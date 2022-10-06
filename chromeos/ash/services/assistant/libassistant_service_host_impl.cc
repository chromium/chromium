// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/libassistant_service_host_impl.h"

#include "base/check.h"
#include "base/sequence_checker.h"
#include "build/buildflag.h"
#include "chromeos/ash/components/assistant/buildflags.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/libassistant_service.h"
#include "chromeos/ash/services/libassistant/public/mojom/service.mojom-forward.h"
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)

namespace ash::assistant {

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)

LibassistantServiceHostImpl::LibassistantServiceHostImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

LibassistantServiceHostImpl::~LibassistantServiceHostImpl() = default;

void LibassistantServiceHostImpl::Launch(
    mojo::PendingReceiver<libassistant::mojom::LibassistantService> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (assistant::features::IsLibAssistantSandboxEnabled()) {
    AssistantBrowserDelegate::Get()->RequestLibassistantService(
        std::move(receiver));
  } else {
    DCHECK(!libassistant_service_);
    libassistant_service_ = std::make_unique<libassistant::LibassistantService>(
        std::move(receiver));
  }
}

void LibassistantServiceHostImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  libassistant_service_ = nullptr;
}

#else

LibassistantServiceHostImpl::LibassistantServiceHostImpl() = default;
LibassistantServiceHostImpl::~LibassistantServiceHostImpl() = default;

void LibassistantServiceHostImpl::Launch(
    mojo::PendingReceiver<libassistant::mojom::LibassistantService> receiver) {}

void LibassistantServiceHostImpl::Stop() {}

#endif

}  // namespace ash::assistant
