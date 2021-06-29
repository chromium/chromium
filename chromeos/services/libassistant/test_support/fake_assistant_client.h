// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_ASSISTANT_CLIENT_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_ASSISTANT_CLIENT_H_

#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/services/libassistant/grpc/assistant_client.h"

namespace chromeos {
namespace libassistant {

class FakeAssistantClient : public AssistantClient {
 public:
  FakeAssistantClient(
      std::unique_ptr<assistant::FakeAssistantManager> assistant_manager,
      assistant::FakeAssistantManagerInternal* assistant_manager_internal);
  ~FakeAssistantClient() override;

  // chromeos::libassistant::AssistantClient:
  assistant::FakeAssistantManager* assistant_manager() {
    return reinterpret_cast<assistant::FakeAssistantManager*>(
        AssistantClient::assistant_manager());
  }
  assistant::FakeAssistantManagerInternal* assistant_manager_internal() {
    return reinterpret_cast<assistant::FakeAssistantManagerInternal*>(
        AssistantClient::assistant_manager_internal());
  }

  bool StartGrpcServices() override;
  void AddExperimentIds(const std::vector<std::string>& exp_ids) override;
  void SendVoicelessInteraction(
      const ::assistant::api::Interaction& interaction,
      const std::string& description,
      const ::assistant::api::VoicelessOptions& options,
      base::OnceCallback<void(bool)> on_done) override;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_ASSISTANT_CLIENT_H_
