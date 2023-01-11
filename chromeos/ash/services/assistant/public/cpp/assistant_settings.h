// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_SETTINGS_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_SETTINGS_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::assistant {

class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) SpeakerIdEnrollmentClient
    : public libassistant::mojom::SpeakerIdEnrollmentClient {
 public:
  SpeakerIdEnrollmentClient();
  ~SpeakerIdEnrollmentClient() override;

  mojo::PendingRemote<libassistant::mojom::SpeakerIdEnrollmentClient>
  BindNewPipeAndPassRemote();

  void ResetReceiver();

 private:
  mojo::Receiver<libassistant::mojom::SpeakerIdEnrollmentClient> client_{this};
};

class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) AssistantSettings {
 public:
  AssistantSettings();
  AssistantSettings(const AssistantSettings&) = delete;
  AssistantSettings& operator=(const AssistantSettings&) = delete;
  virtual ~AssistantSettings();

  static AssistantSettings* Get();

  // |selector| is a serialized proto of SettingsUiSelector, indicating which
  // settings sub-pages should be requested to the server.
  // For `GetSettings`, a serialized proto of SettingsUi containing the settings
  // sub-pages requested is paased to `GetSettingsCallback`.
  // For `GetSettingsWithHeader`, a serialized proto of GetSettingsUiResponse
  // containing the settings and the header information is passed to
  // `GetSettingsCallback`.
  // Send a request for the settings ui sub-pages indicated by the |selector|.
  using GetSettingsCallback = base::OnceCallback<void(const std::string&)>;
  virtual void GetSettings(const std::string& selector,
                           GetSettingsCallback callback) = 0;
  virtual void GetSettingsWithHeader(const std::string& selector,
                                     GetSettingsCallback callback) = 0;

  // |update| is a serialized proto of SettingsUiUpdate, indicating what kind
  // of updates should be applied to the settings.
  // Return value is a serialized proto of SettingsUiUpdateResult, containing
  // the result of updates.
  // Send a request to update the assistant settings indicated by the |update|.
  using UpdateSettingsCallback = base::OnceCallback<void(const std::string&)>;
  virtual void UpdateSettings(const std::string& update,
                              UpdateSettingsCallback callback) = 0;

  // Starts speaker id enrollment.
  // |skip_cloud_enrollment| whether to skip Cloud Enrollment (e.g. for when
  // user explicictly requests voice match re-training).
  virtual void StartSpeakerIdEnrollment(
      bool skip_cloud_enrollment,
      base::WeakPtr<SpeakerIdEnrollmentClient> client) = 0;

  // Stops speaker id enrollment.
  virtual void StopSpeakerIdEnrollment() = 0;

  // Sync speaker id enrollment status.
  virtual void SyncSpeakerIdEnrollmentStatus() = 0;
};

}  // namespace ash::assistant

// TODO(b/258750971): remove when internal assistant codes are migrated to
// namespace ash.
namespace chromeos::assistant {
using ::ash::assistant::AssistantSettings;
using ::ash::assistant::SpeakerIdEnrollmentClient;
}  // namespace chromeos::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_SETTINGS_H_
