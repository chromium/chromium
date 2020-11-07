// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_
#define CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_

#include <memory>

#include "base/threading/thread.h"

namespace chromeos {
namespace assistant {

class ServiceController;

// The proxy to the Assistant service, which serves as the main
// access point to the entire Assistant API.
class AssistantProxy {
 public:
  AssistantProxy();
  AssistantProxy(AssistantProxy&) = delete;
  AssistantProxy& operator=(AssistantProxy&) = delete;
  ~AssistantProxy();

  // Returns the controller that manages starting and stopping of the Assistant
  // service.
  ServiceController& service_controller();

  // The background thread is temporary exposed until the entire Libassistant
  // API is hidden behind this proxy API.
  base::Thread& background_thread() { return background_thread_; }

 private:
  base::Thread background_thread_{"Assistant background thread"};

  std::unique_ptr<ServiceController> service_controller_;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_
