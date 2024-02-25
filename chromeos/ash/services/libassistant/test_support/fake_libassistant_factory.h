// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_LIBASSISTANT_FACTORY_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_LIBASSISTANT_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/libassistant/libassistant_factory.h"

#include "base/component_export.h"

namespace chromeos {
namespace assistant {
class FakeAssistantManager;
}  // namespace assistant
}  // namespace chromeos

namespace ash::libassistant {

// Implementation of |LibassistantFactory| that returns fake
// instances for all of the member methods. Used during unittests.
class FakeLibassistantFactory : public LibassistantFactory {
 public:
  FakeLibassistantFactory();
  ~FakeLibassistantFactory() override;

  chromeos::assistant::FakeAssistantManager& assistant_manager();

  // LibassistantFactory implementation:
  std::unique_ptr<assistant_client::AssistantManager> CreateAssistantManager(
      const std::string& libassistant_config) override;

  std::string libassistant_config() const { return libassistant_config_; }

 private:
  std::unique_ptr<chromeos::assistant::FakeAssistantManager>
      pending_assistant_manager_;
  raw_ptr<chromeos::assistant::FakeAssistantManager>
      current_assistant_manager_ = nullptr;

  // Config passed to LibAssistant when it was started.
  std::string libassistant_config_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_LIBASSISTANT_FACTORY_H_
