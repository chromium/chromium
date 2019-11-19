// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_client.h"

#include <utility>

#include "ash/public/cpp/assistant/assistant_interface_binder.h"
#include "ash/public/cpp/network_config_service.h"
#include "chrome/browser/chromeos/assistant/assistant_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/assistant/assistant_context_util.h"
#include "chrome/browser/ui/ash/assistant/assistant_image_downloader.h"
#include "chrome/browser/ui/ash/assistant/assistant_service_connection.h"
#include "chrome/browser/ui/ash/assistant/assistant_setup.h"
#include "chrome/browser/ui/ash/assistant/proactive_suggestions_client_impl.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/services/assistant/public/features.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_switches.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/identity/public/mojom/identity_service.mojom.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/preferences/public/mojom/preferences.mojom.h"

namespace {

// Owned by ChromeBrowserMainChromeOS:
AssistantClient* g_instance = nullptr;

}  // namespace

// static
AssistantClient* AssistantClient::Get() {
  DCHECK(g_instance);
  return g_instance;
}

AssistantClient::AssistantClient() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;

  auto* session_manager = session_manager::SessionManager::Get();
  // AssistantClient must be created before any user session is created.
  // Otherwise, it will not get OnUserProfileLoaded notification.
  DCHECK(session_manager->sessions().empty());
  session_manager->AddObserver(this);
}

AssistantClient::~AssistantClient() {
  DCHECK(g_instance);
  g_instance = nullptr;

  session_manager::SessionManager::Get()->RemoveObserver(this);
  if (identity_manager_)
    identity_manager_->RemoveObserver(this);
}

void AssistantClient::MaybeInit(Profile* profile) {
  if (assistant::IsAssistantAllowedForProfile(profile) !=
      ash::mojom::AssistantAllowedState::ALLOWED) {
    return;
  }

  if (!profile_) {
    profile_ = profile;
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
    DCHECK(identity_manager_);
    identity_manager_->AddObserver(this);
  }
  DCHECK_EQ(profile_, profile);

  if (initialized_)
    return;

  initialized_ = true;

  bool is_test = base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kBrowserTest);
  auto* service =
      AssistantServiceConnection::GetForProfile(profile_)->service();
  service->Init(client_receiver_.BindNewPipeAndPassRemote(),
                device_actions_.AddReceiver(), is_test);
  assistant_image_downloader_ = std::make_unique<AssistantImageDownloader>();
  assistant_setup_ = std::make_unique<AssistantSetup>(service);

  if (chromeos::assistant::features::IsProactiveSuggestionsEnabled()) {
    proactive_suggestions_client_ =
        std::make_unique<ProactiveSuggestionsClientImpl>(profile_);
  }

  for (auto& receiver : pending_assistant_receivers_)
    service->BindAssistant(std::move(receiver));
  pending_assistant_receivers_.clear();
}

void AssistantClient::MaybeStartAssistantOptInFlow() {
  if (!initialized_)
    return;

  assistant_setup_->MaybeStartAssistantOptInFlow();
}

void AssistantClient::BindAssistant(
    mojo::PendingReceiver<chromeos::assistant::mojom::Assistant> receiver) {
  if (!initialized_) {
    pending_assistant_receivers_.push_back(std::move(receiver));
    return;
  }

  AssistantServiceConnection::GetForProfile(profile_)->service()->BindAssistant(
      std::move(receiver));
}

void AssistantClient::OnAssistantStatusChanged(
    ash::mojom::AssistantState new_state) {
  ash::AssistantState::Get()->NotifyStatusChanged(new_state);
}

void AssistantClient::RequestAssistantStructure(
    RequestAssistantStructureCallback callback) {
  RequestAssistantStructureForActiveBrowserWindow(std::move(callback));
}

void AssistantClient::RequestAssistantController(
    mojo::PendingReceiver<chromeos::assistant::mojom::AssistantController>
        receiver) {
  ash::AssistantInterfaceBinder::GetInstance()->BindController(
      std::move(receiver));
}

void AssistantClient::RequestAssistantAlarmTimerController(
    mojo::PendingReceiver<ash::mojom::AssistantAlarmTimerController> receiver) {
  ash::AssistantInterfaceBinder::GetInstance()->BindAlarmTimerController(
      std::move(receiver));
}

void AssistantClient::RequestAssistantNotificationController(
    mojo::PendingReceiver<ash::mojom::AssistantNotificationController>
        receiver) {
  ash::AssistantInterfaceBinder::GetInstance()->BindNotificationController(
      std::move(receiver));
}

void AssistantClient::RequestAssistantScreenContextController(
    mojo::PendingReceiver<ash::mojom::AssistantScreenContextController>
        receiver) {
  ash::AssistantInterfaceBinder::GetInstance()->BindScreenContextController(
      std::move(receiver));
}

void AssistantClient::RequestAssistantVolumeControl(
    mojo::PendingReceiver<ash::mojom::AssistantVolumeControl> receiver) {
  ash::AssistantInterfaceBinder::GetInstance()->BindVolumeControl(
      std::move(receiver));
}

void AssistantClient::RequestAssistantStateController(
    mojo::PendingReceiver<ash::mojom::AssistantStateController> receiver) {
  ash::AssistantInterfaceBinder::GetInstance()->BindStateController(
      std::move(receiver));
}

void AssistantClient::RequestPrefStoreConnector(
    mojo::PendingReceiver<prefs::mojom::PrefStoreConnector> receiver) {
  content::BrowserContext::GetConnectorFor(profile_)->Connect(
      prefs::mojom::kServiceName, std::move(receiver));
}

void AssistantClient::RequestBatteryMonitor(
    mojo::PendingReceiver<device::mojom::BatteryMonitor> receiver) {
  content::GetSystemConnector()->Connect(device::mojom::kServiceName,
                                         std::move(receiver));
}

void AssistantClient::RequestWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  content::GetSystemConnector()->Connect(device::mojom::kServiceName,
                                         std::move(receiver));
}

void AssistantClient::RequestAudioStreamFactory(
    mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) {
  content::GetSystemConnector()->Connect(audio::mojom::kServiceName,
                                         std::move(receiver));
}

void AssistantClient::RequestAudioDecoderFactory(
    mojo::PendingReceiver<
        chromeos::assistant::mojom::AssistantAudioDecoderFactory> receiver) {
  content::ServiceProcessHost::Launch(
      std::move(receiver),
      content::ServiceProcessHost::Options()
          .WithSandboxType(service_manager::SANDBOX_TYPE_UTILITY)
          .WithDisplayName("Assistant Audio Decoder Service")
          .Pass());
}

void AssistantClient::RequestIdentityAccessor(
    mojo::PendingReceiver<identity::mojom::IdentityAccessor> receiver) {
  identity::mojom::IdentityService* service = profile_->GetIdentityService();
  if (service)
    service->BindIdentityAccessor(std::move(receiver));
}

void AssistantClient::RequestAudioFocusManager(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver) {
  content::GetSystemConnector()->Connect(media_session::mojom::kServiceName,
                                         std::move(receiver));
}

void AssistantClient::RequestMediaControllerManager(
    mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
        receiver) {
  content::GetSystemConnector()->Connect(media_session::mojom::kServiceName,
                                         std::move(receiver));
}

void AssistantClient::RequestNetworkConfig(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  ash::GetNetworkConfigService(std::move(receiver));
}

void AssistantClient::OnExtendedAccountInfoUpdated(const AccountInfo& info) {
  if (initialized_)
    return;

  MaybeInit(profile_);
}

void AssistantClient::OnUserProfileLoaded(const AccountId& account_id) {
  // Initialize Assistant when primary user profile is loaded so that it could
  // be used in post oobe steps. OnUserSessionStarted() is too late
  // because it happens after post oobe steps
  Profile* user_profile =
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (!chromeos::ProfileHelper::IsPrimaryProfile(user_profile))
    return;

  MaybeInit(user_profile);
}

void AssistantClient::OnUserSessionStarted(bool is_primary_user) {
  if (is_primary_user && !chromeos::switches::ShouldSkipOobePostLogin())
    MaybeStartAssistantOptInFlow();
}
