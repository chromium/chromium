// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_V1_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_V1_H_

#include "chromeos/services/libassistant/grpc/assistant_client.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"

namespace chromeos {
namespace libassistant {

class AssistantClientV1 : public AssistantClient {
 public:
  AssistantClientV1(
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal);
  ~AssistantClientV1() override;

  // chromeos::libassistant::AssistantClient:
  bool StartGrpcServices() override;
  void AddExperimentIds(const std::vector<std::string>& exp_ids) override;
  void SendVoicelessInteraction(
      const ::assistant::api::Interaction& interaction,
      const std::string& description,
      const ::assistant::api::VoicelessOptions& options,
      base::OnceCallback<void(bool)> on_done) override;
  void StartSpeakerIdEnrollment(
      const StartSpeakerIdEnrollmentRequest& request,
      base::RepeatingCallback<void(const SpeakerIdEnrollmentEvent&)> on_done)
      override;
  void CancelSpeakerIdEnrollment(
      const CancelSpeakerIdEnrollmentRequest& request) override;
  void GetSpeakerIdEnrollmentInfo(
      const GetSpeakerIdEnrollmentInfoRequest& request,
      base::OnceCallback<void(bool user_model_exists)> on_done) override;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_V1_H_
