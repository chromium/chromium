// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_LIBASSISTANT_FACTORY_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_LIBASSISTANT_FACTORY_H_

#include <memory>
#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
}  // namespace assistant_client

namespace ash::libassistant {

// Factory class that creates the main actual Libassistant classes.
// Can be replaced with a fake for unittests.
class LibassistantFactory {
 public:
  virtual ~LibassistantFactory() = default;

  virtual std::unique_ptr<assistant_client::AssistantManager>
  CreateAssistantManager(const std::string& lib_assistant_config) = 0;

  virtual assistant_client::AssistantManagerInternal*
  UnwrapAssistantManagerInternal(
      assistant_client::AssistantManager* assistant_manager) = 0;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_LIBASSISTANT_FACTORY_H_
