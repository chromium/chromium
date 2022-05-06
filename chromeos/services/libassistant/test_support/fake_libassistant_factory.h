// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_LIBASSISTANT_FACTORY_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_LIBASSISTANT_FACTORY_H_

#include "chromeos/services/libassistant/libassistant_factory.h"

#include "base/component_export.h"

namespace chromeos {
namespace assistant {
class FakeAssistantManager;
class FakeAssistantManagerInternal;
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace libassistant {

// Implementation of |LibassistantFactory| that returns fake
// instances for all of the member methods. Used during unittests.
class FakeLibassistantFactory : public LibassistantFactory {
 public:
  FakeLibassistantFactory();
  ~FakeLibassistantFactory() override;

  assistant::FakeAssistantManager& assistant_manager();
  assistant::FakeAssistantManagerInternal& assistant_manager_internal();

  // LibassistantFactory implementation:
  std::unique_ptr<assistant_client::AssistantManager> CreateAssistantManager(
      const std::string& libassistant_config) override;
  assistant_client::AssistantManagerInternal* UnwrapAssistantManagerInternal(
      assistant_client::AssistantManager* assistant_manager) override;
  void LoadLibassistantLibraryFromDlc(const std::string& root_path) override;

  std::string libassistant_config() const { return libassistant_config_; }

 private:
  std::unique_ptr<assistant::FakeAssistantManager> pending_assistant_manager_;
  assistant::FakeAssistantManager* current_assistant_manager_ = nullptr;

  // Config passed to LibAssistant when it was started.
  std::string libassistant_config_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_LIBASSISTANT_FACTORY_H_
