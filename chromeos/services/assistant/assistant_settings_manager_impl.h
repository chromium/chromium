// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_SETTINGS_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_SETTINGS_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "chromeos/services/assistant/assistant_settings_manager.h"
#include "chromeos/services/assistant/public/mojom/settings.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
class AssistantStateBase;
}  // namespace ash

namespace assistant_client {
struct SpeakerIdEnrollmentStatus;
struct SpeakerIdEnrollmentUpdate;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {
namespace mojom {
class AssistantController;
}  // namespace mojom
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace assistant {

class AssistantManagerServiceImpl;
class ServiceContext;

class AssistantSettingsManagerImpl : public AssistantSettingsManager {
 public:
  AssistantSettingsManagerImpl(
      ServiceContext* context,
      AssistantManagerServiceImpl* assistant_manager_service);
  ~AssistantSettingsManagerImpl() override;

  bool speaker_id_enrollment_done() { return speaker_id_enrollment_done_; }

  // AssistantSettingsManager overrides:
  void BindReceiver(
      mojo::PendingReceiver<mojom::AssistantSettingsManager> receiver) override;

  // mojom::AssistantSettingsManager overrides:
  void GetSettings(const std::string& selector,
                   GetSettingsCallback callback) override;
  void UpdateSettings(const std::string& update,
                      UpdateSettingsCallback callback) override;
  void StartSpeakerIdEnrollment(
      bool skip_cloud_enrollment,
      mojo::PendingRemote<mojom::SpeakerIdEnrollmentClient> client) override;
  void StopSpeakerIdEnrollment(
      StopSpeakerIdEnrollmentCallback callback) override;
  void SyncSpeakerIdEnrollmentStatus() override;

  void SyncDeviceAppsStatus(base::OnceCallback<void(bool)> callback);

  void UpdateServerDeviceSettings();

 private:
  void HandleSpeakerIdEnrollmentUpdate(
      const assistant_client::SpeakerIdEnrollmentUpdate& update);
  void HandleStopSpeakerIdEnrollment(base::RepeatingCallback<void()> callback);
  void HandleSpeakerIdEnrollmentStatusSync(
      const assistant_client::SpeakerIdEnrollmentStatus& status);
  void HandleDeviceAppsStatusSync(base::OnceCallback<void(bool)> callback,
                                  const std::string& settings);

  ash::AssistantStateBase* assistant_state();
  mojom::AssistantController* assistant_controller();
  scoped_refptr<base::SequencedTaskRunner> main_task_runner();

  ServiceContext* const context_;
  AssistantManagerServiceImpl* const assistant_manager_service_;
  mojo::Remote<mojom::SpeakerIdEnrollmentClient> speaker_id_enrollment_client_;

  // Whether the speaker id enrollment has complete for the user.
  bool speaker_id_enrollment_done_ = false;

  mojo::ReceiverSet<mojom::AssistantSettingsManager> receivers_;

  base::WeakPtrFactory<AssistantSettingsManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantSettingsManagerImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_SETTINGS_MANAGER_IMPL_H_
