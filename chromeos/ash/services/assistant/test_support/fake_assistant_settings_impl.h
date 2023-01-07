// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_ASSISTANT_SETTINGS_IMPL_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_ASSISTANT_SETTINGS_IMPL_H_

#include <memory>
#include <string>

#include "chromeos/ash/services/assistant/public/cpp/assistant_settings.h"

namespace ash::assistant {

// TODO(jeroendh): Can be removed once FakeAssistantManagerServiceImpl is gone.
class FakeAssistantSettingsImpl : public AssistantSettings {
 public:
  FakeAssistantSettingsImpl();
  ~FakeAssistantSettingsImpl() override;

  // AssistantSettings overrides:
  void GetSettings(const std::string& selector,
                   GetSettingsCallback callback) override;
  void GetSettingsWithHeader(const std::string& selector,
                             GetSettingsCallback callback) override;
  void UpdateSettings(const std::string& update,
                      UpdateSettingsCallback callback) override;
  void StartSpeakerIdEnrollment(
      bool skip_cloud_enrollment,
      base::WeakPtr<SpeakerIdEnrollmentClient> client) override;
  void StopSpeakerIdEnrollment() override;
  void SyncSpeakerIdEnrollmentStatus() override {}
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_ASSISTANT_SETTINGS_IMPL_H_
