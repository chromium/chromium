// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_LIBASSISTANT_SERVICE_HOST_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_LIBASSISTANT_SERVICE_HOST_IMPL_H_

#include <memory>

#include "base/synchronization/lock.h"
#include "chromeos/services/assistant/proxy/libassistant_service_host.h"

namespace chromeos {
namespace libassistant {
class LibassistantService;
}  // namespace libassistant
}  // namespace chromeos

namespace chromeos {
namespace assistant {

class AssistantManagerServiceDelegate;
class CrosPlatformApi;

class LibassistantServiceHostImpl : public LibassistantServiceHost {
 public:
  using InitializeCallback =
      base::OnceCallback<void(assistant_client::AssistantManager*,
                              assistant_client::AssistantManagerInternal*)>;

  LibassistantServiceHostImpl(CrosPlatformApi* platform_api,
                              AssistantManagerServiceDelegate* delegate);
  LibassistantServiceHostImpl(LibassistantServiceHostImpl&) = delete;
  LibassistantServiceHostImpl& operator=(LibassistantServiceHostImpl&) = delete;
  ~LibassistantServiceHostImpl() override;

  // LibassistantServiceHostImpl implementation:
  void Launch(
      mojo::PendingReceiver<LibassistantServiceMojom> receiver) override;
  void Stop() override;
  void SetInitializeCallback(InitializeCallback) override;

 private:
  // Owned by |AssistantManagerServiceImpl| which also owns |this|.
  CrosPlatformApi* const platform_api_;
  // Owned by |AssistantManagerServiceImpl| which also owns |this|.
  AssistantManagerServiceDelegate* const delegate_;

  // Protects access to |libassistant_service_|. This is required because the
  // service will be launched/stopped on a background thread, where the other
  // methods will be called from the main thread.
  base::Lock libassistant_service_lock_;
  std::unique_ptr<chromeos::libassistant::LibassistantService>
      libassistant_service_ GUARDED_BY(libassistant_service_lock_);
  // Used when SetInitializeCallback() is called before Launch().
  InitializeCallback pending_initialize_callback_;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_LIBASSISTANT_SERVICE_HOST_IMPL_H_
