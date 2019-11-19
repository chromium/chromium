// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_CLIENT_H_

#include <memory>
#include <vector>

#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "base/macros.h"
#include "chrome/browser/ui/ash/assistant/device_actions.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class AssistantImageDownloader;
class AssistantSetup;
class ProactiveSuggestionsClientImpl;
class Profile;

// Class to handle all Assistant in-browser-process functionalities.
class AssistantClient : chromeos::assistant::mojom::Client,
                        public signin::IdentityManager::Observer,
                        public session_manager::SessionManagerObserver {
 public:
  static AssistantClient* Get();

  AssistantClient();
  ~AssistantClient() override;

  void MaybeInit(Profile* profile);
  void MaybeStartAssistantOptInFlow();

  void BindAssistant(
      mojo::PendingReceiver<chromeos::assistant::mojom::Assistant> receiver);

  // assistant::mojom::Client overrides:
  void OnAssistantStatusChanged(ash::mojom::AssistantState new_state) override;
  void RequestAssistantStructure(
      RequestAssistantStructureCallback callback) override;
  void RequestAssistantController(
      mojo::PendingReceiver<chromeos::assistant::mojom::AssistantController>
          receiver) override;
  void RequestAssistantAlarmTimerController(
      mojo::PendingReceiver<ash::mojom::AssistantAlarmTimerController> receiver)
      override;
  void RequestAssistantNotificationController(
      mojo::PendingReceiver<ash::mojom::AssistantNotificationController>
          receiver) override;
  void RequestAssistantScreenContextController(
      mojo::PendingReceiver<ash::mojom::AssistantScreenContextController>
          receiver) override;
  void RequestAssistantVolumeControl(
      mojo::PendingReceiver<ash::mojom::AssistantVolumeControl> receiver)
      override;
  void RequestAssistantStateController(
      mojo::PendingReceiver<ash::mojom::AssistantStateController> receiver)
      override;
  void RequestPrefStoreConnector(
      mojo::PendingReceiver<prefs::mojom::PrefStoreConnector> receiver)
      override;
  void RequestBatteryMonitor(
      mojo::PendingReceiver<device::mojom::BatteryMonitor> receiver) override;
  void RequestWakeLockProvider(
      mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) override;
  void RequestAudioStreamFactory(
      mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) override;
  void RequestAudioDecoderFactory(
      mojo::PendingReceiver<
          chromeos::assistant::mojom::AssistantAudioDecoderFactory> receiver)
      override;
  void RequestIdentityAccessor(
      mojo::PendingReceiver<identity::mojom::IdentityAccessor> receiver)
      override;
  void RequestAudioFocusManager(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver)
      override;
  void RequestMediaControllerManager(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) override;
  void RequestNetworkConfig(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver) override;

 private:
  // signin::IdentityManager::Observer:
  // Retry to initiate Assistant service when account info has been updated.
  // This is necessary if previous calls of MaybeInit() failed due to Assistant
  // disallowed by account type. This can happen when the chromeos sign-in
  // finished before account info fetching is finished (|hosted_domain| field
  // will be empty under this case).
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;
  void OnUserSessionStarted(bool is_primary_user) override;

  mojo::Receiver<chromeos::assistant::mojom::Client> client_receiver_{this};

  DeviceActions device_actions_;

  std::unique_ptr<AssistantImageDownloader> assistant_image_downloader_;
  std::unique_ptr<AssistantSetup> assistant_setup_;

  std::unique_ptr<ProactiveSuggestionsClientImpl> proactive_suggestions_client_;

  // Assistant interface receivers to be bound once we're initialized. These
  // accumulate when BindAssistant is called before initialization.
  std::vector<mojo::PendingReceiver<chromeos::assistant::mojom::Assistant>>
      pending_assistant_receivers_;

  bool initialized_ = false;

  // Non-owning pointers.
  Profile* profile_ = nullptr;
  signin::IdentityManager* identity_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantClient);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_CLIENT_H_
