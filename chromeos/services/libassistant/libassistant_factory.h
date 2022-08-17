// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_FACTORY_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_FACTORY_H_

#include <memory>
#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
}  // namespace assistant_client

namespace chromeos {
namespace libassistant {

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

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_FACTORY_H_
