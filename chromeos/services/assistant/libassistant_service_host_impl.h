// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_LIBASSISTANT_SERVICE_HOST_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_LIBASSISTANT_SERVICE_HOST_IMPL_H_

#include <memory>

#include "chromeos/services/assistant/proxy/libassistant_service_host.h"

namespace assistant_client {
class PlatformApi;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

class AssistantManagerServiceDelegate;

class LibassistantServiceHostImpl : public LibassistantServiceHost {
 public:
  LibassistantServiceHostImpl(assistant_client::PlatformApi* platform_api,
                              AssistantManagerServiceDelegate* delegate);
  LibassistantServiceHostImpl(LibassistantServiceHostImpl&) = delete;
  LibassistantServiceHostImpl& operator=(LibassistantServiceHostImpl&) = delete;
  ~LibassistantServiceHostImpl() override;

  // LibassistantServiceHostImpl implementation:
  void Launch(
      mojo::PendingReceiver<LibassistantServiceMojom> receiver) override;
  void Stop() override;

 private:
  // Owned by |AssistantManagerServiceImpl| which also owns |this|.
  assistant_client::PlatformApi* const platform_api_;
  // Owned by |AssistantManagerServiceImpl| which also owns |this|.
  AssistantManagerServiceDelegate* const delegate_;

  std::unique_ptr<LibassistantServiceMojom> libassistant_service_;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_LIBASSISTANT_SERVICE_HOST_IMPL_H_
