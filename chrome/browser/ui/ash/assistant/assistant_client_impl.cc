// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_client_impl.h"

#include <utility>

#include "ash/public/cpp/assistant/assistant_interface_binder.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/network_config_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/assistant/assistant_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/assistant/assistant_context_util.h"
#include "chrome/browser/ui/ash/assistant/assistant_setup.h"
#include "chrome/browser/ui/ash/assistant/assistant_web_view_factory_impl.h"
#include "chrome/browser/ui/ash/assistant/conversation_starters_client_impl.h"
#include "chrome/browser/ui/ash/assistant/device_actions_delegate_impl.h"
#include "chromeos/components/bloom/public/cpp/bloom_controller.h"
#include "chromeos/components/bloom/public/cpp/bloom_controller_factory.h"
#include "chromeos/components/bloom/public/cpp/bloom_screenshot_delegate.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

AssistantClientImpl::AssistantClientImpl() {
  auto* session_manager = session_manager::SessionManager::Get();
  // AssistantClientImpl must be created before any user session is created.
  // Otherwise, it will not get OnUserProfileLoaded notification.
  DCHECK(session_manager->sessions().empty());
  session_manager->AddObserver(this);

  notification_registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                              content::NotificationService::AllSources());
}

AssistantClientImpl::~AssistantClientImpl() {
  session_manager::SessionManager::Get()->RemoveObserver(this);
  if (identity_manager_)
    identity_manager_->RemoveObserver(this);
}

void AssistantClientImpl::MaybeInit(Profile* profile) {
  if (assistant::IsAssistantAllowedForProfile(profile) !=
      chromeos::assistant::AssistantAllowedState::ALLOWED) {
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

  device_actions_ = std::make_unique<DeviceActions>(
      std::make_unique<DeviceActionsDelegateImpl>());

  service_ = std::make_unique<chromeos::assistant::Service>(
      profile->GetURLLoaderFactory()->Clone(),
      IdentityManagerFactory::GetForProfile(profile));
  service_->Init();

  assistant_setup_ = std::make_unique<AssistantSetup>();
  assistant_web_view_factory_ =
      std::make_unique<AssistantWebViewFactoryImpl>(profile_);

  if (chromeos::assistant::features::IsConversationStartersV2Enabled()) {
    conversation_starters_client_ =
        std::make_unique<ConversationStartersClientImpl>(profile_);
  }

  if (chromeos::assistant::features::IsBloomEnabled()) {
    bloom_controller_ = chromeos::bloom::BloomControllerFactory::Create(
        profile->GetURLLoaderFactory()->Clone(),
        IdentityManagerFactory::GetForProfile(profile));
  }
}

void AssistantClientImpl::MaybeStartAssistantOptInFlow() {
  if (!initialized_)
    return;

  assistant_setup_->MaybeStartAssistantOptInFlow();
}

void AssistantClientImpl::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  if (!initialized_)
    return;

  chromeos::assistant::AssistantService::Get()->Shutdown();
}

void AssistantClientImpl::RequestAssistantStructure(
    RequestAssistantStructureCallback callback) {
  RequestAssistantStructureForActiveBrowserWindow(std::move(callback));
}

void AssistantClientImpl::OnAssistantStatusChanged(
    chromeos::assistant::AssistantStatus new_status) {
  ash::AssistantState::Get()->NotifyStatusChanged(new_status);
}

void AssistantClientImpl::RequestAssistantVolumeControl(
    mojo::PendingReceiver<ash::mojom::AssistantVolumeControl> receiver) {
  ash::AssistantInterfaceBinder::GetInstance()->BindVolumeControl(
      std::move(receiver));
}

void AssistantClientImpl::RequestBatteryMonitor(
    mojo::PendingReceiver<device::mojom::BatteryMonitor> receiver) {
  content::GetDeviceService().BindBatteryMonitor(std::move(receiver));
}

void AssistantClientImpl::RequestWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  content::GetDeviceService().BindWakeLockProvider(std::move(receiver));
}

void AssistantClientImpl::RequestAudioStreamFactory(
    mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) {
  content::GetAudioService().BindStreamFactory(std::move(receiver));
}

void AssistantClientImpl::RequestAudioDecoderFactory(
    mojo::PendingReceiver<
        chromeos::assistant::mojom::AssistantAudioDecoderFactory> receiver) {
  content::ServiceProcessHost::Launch(
      std::move(receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Assistant Audio Decoder Service")
          .Pass());
}

void AssistantClientImpl::RequestAudioFocusManager(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver) {
  content::GetMediaSessionService().BindAudioFocusManager(std::move(receiver));
}

void AssistantClientImpl::RequestMediaControllerManager(
    mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
        receiver) {
  content::GetMediaSessionService().BindMediaControllerManager(
      std::move(receiver));
}

void AssistantClientImpl::RequestNetworkConfig(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  ash::GetNetworkConfigService(std::move(receiver));
}

void AssistantClientImpl::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (initialized_)
    return;

  MaybeInit(profile_);
}

void AssistantClientImpl::OnUserProfileLoaded(const AccountId& account_id) {
  if (!assistant_state_observer_.IsObservingSources() && !initialized_ &&
      ash::AssistantState::Get()) {
    assistant_state_observer_.Add(ash::AssistantState::Get());
  }
}

void AssistantClientImpl::OnUserSessionStarted(bool is_primary_user) {
  // Disable the handling for browser tests to prevent the Assistant being
  // enabled unexpectedly.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (is_primary_user && !chromeos::switches::ShouldSkipOobePostLogin() &&
      !command_line->HasSwitch(switches::kBrowserTest)) {
    MaybeStartAssistantOptInFlow();
  }
}

void AssistantClientImpl::OnAssistantFeatureAllowedChanged(
    chromeos::assistant::AssistantAllowedState allowed_state) {
  if (allowed_state != chromeos::assistant::AssistantAllowedState::ALLOWED)
    return;

  Profile* profile = ProfileManager::GetActiveUserProfile();
  MaybeInit(profile);
}
