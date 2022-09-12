// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_LIBASSISTANT_SERVICE_HOST_IMPL_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_LIBASSISTANT_SERVICE_HOST_IMPL_H_

#include <memory>

#include "base/thread_annotations.h"
#include "build/buildflag.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/services/assistant/libassistant_service_host.h"

namespace chromeos {
namespace libassistant {
class LibassistantService;
}  // namespace libassistant
}  // namespace chromeos

namespace ash::assistant {

// Host class controlling the lifetime of the Libassistant service.
// The implementation will be stubbed out in the unbranded build.
class LibassistantServiceHostImpl : public LibassistantServiceHost {
 public:
  LibassistantServiceHostImpl();
  LibassistantServiceHostImpl(LibassistantServiceHostImpl&) = delete;
  LibassistantServiceHostImpl& operator=(LibassistantServiceHostImpl&) = delete;
  ~LibassistantServiceHostImpl() override;

  // LibassistantServiceHost implementation:
  void Launch(
      mojo::PendingReceiver<chromeos::libassistant::mojom::LibassistantService>
          receiver) override;
  void Stop() override;

 private:
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<chromeos::libassistant::LibassistantService>
      libassistant_service_ GUARDED_BY_CONTEXT(sequence_checker_);
#endif
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_LIBASSISTANT_SERVICE_HOST_IMPL_H_
