// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/test_support/fake_assistant_client.h"

#include "base/callback.h"

namespace chromeos {
namespace libassistant {

FakeAssistantClient::FakeAssistantClient(
    std::unique_ptr<assistant::FakeAssistantManager> assistant_manager,
    assistant::FakeAssistantManagerInternal* assistant_manager_internal)
    : AssistantClient(std::move(assistant_manager),
                      assistant_manager_internal) {}

FakeAssistantClient::~FakeAssistantClient() = default;

void FakeAssistantClient::StartServices() {}

void FakeAssistantClient::SetChromeOSApiDelegate(
    assistant_client::ChromeOSApiDelegate* delegate) {}

bool FakeAssistantClient::StartGrpcServices() {
  return true;
}

void FakeAssistantClient::AddExperimentIds(
    const std::vector<std::string>& exp_ids) {}

void FakeAssistantClient::SendVoicelessInteraction(
    const ::assistant::api::Interaction& interaction,
    const std::string& description,
    const ::assistant::api::VoicelessOptions& options,
    base::OnceCallback<void(bool)> on_done) {}

void FakeAssistantClient::AddSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) {}

void FakeAssistantClient::RemoveSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) {}

void FakeAssistantClient::StartSpeakerIdEnrollment(
    const StartSpeakerIdEnrollmentRequest& request) {}

void FakeAssistantClient::CancelSpeakerIdEnrollment(
    const CancelSpeakerIdEnrollmentRequest& request) {}

void FakeAssistantClient::GetSpeakerIdEnrollmentInfo(
    const ::assistant::api::GetSpeakerIdEnrollmentInfoRequest& request,
    base::OnceCallback<void(bool user_model_exists)> on_done) {}

void FakeAssistantClient::ResetAllDataAndShutdown() {}

void FakeAssistantClient::OnDisplayRequest(
    const OnDisplayRequestRequest& request) {}

void FakeAssistantClient::AddDisplayEventObserver(
    GrpcServicesObserver<OnAssistantDisplayEventRequest>* observer) {}

void FakeAssistantClient::ResumeCurrentStream() {}

void FakeAssistantClient::PauseCurrentStream() {}

void FakeAssistantClient::SetExternalPlaybackState(
    const MediaStatus& status_proto) {}

void FakeAssistantClient::AddDeviceStateEventObserver(
    GrpcServicesObserver<OnDeviceStateEventRequest>* observer) {}

}  // namespace libassistant
}  // namespace chromeos
