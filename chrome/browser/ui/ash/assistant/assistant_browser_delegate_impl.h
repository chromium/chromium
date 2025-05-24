// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_BROWSER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_BROWSER_DELEGATE_IMPL_H_

#include <memory>
#include <vector>

#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/ash/assistant/device_actions.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class AssistantSetup;
class Profile;

// Class to handle all Assistant in-browser-process functionalities.
class AssistantBrowserDelegateImpl
    : public ash::assistant::AssistantBrowserDelegate,
      public signin::IdentityManager::Observer,
      public session_manager::SessionManagerObserver,
      public ash::AssistantStateObserver {
 public:
  AssistantBrowserDelegateImpl();
  AssistantBrowserDelegateImpl(const AssistantBrowserDelegateImpl&) = delete;
  AssistantBrowserDelegateImpl& operator=(const AssistantBrowserDelegateImpl&) =
      delete;
  ~AssistantBrowserDelegateImpl() override;

  void MaybeStartAssistantOptInFlow();

  // chromeos::assistant::AssistantBrowserDelegate overrides:
  void OnAssistantStatusChanged(
      ash::assistant::AssistantStatus new_status) override;
  void RequestAssistantVolumeControl(
      mojo::PendingReceiver<ash::mojom::AssistantVolumeControl> receiver)
      override;
  void RequestBatteryMonitor(
      mojo::PendingReceiver<device::mojom::BatteryMonitor> receiver) override;
  void RequestWakeLockProvider(
      mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) override;
  void RequestAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver)
      override;
  void RequestAudioDecoderFactory(
      mojo::PendingReceiver<ash::assistant::mojom::AssistantAudioDecoderFactory>
          receiver) override;
  void RequestAudioFocusManager(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver)
      override;
  void RequestMediaControllerManager(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) override;
  void RequestNetworkConfig(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver) override;
  void OpenUrl(GURL url) override;
  base::expected<bool, AssistantBrowserDelegate::Error>
  IsNewEntryPointEligibleForPrimaryProfile() override;
  void OpenNewEntryPoint() override;
  std::optional<std::string> GetNewEntryPointName() override;
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  void RequestLibassistantService(
      mojo::PendingReceiver<ash::libassistant::mojom::LibassistantService>
          receiver) override;
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)

  void OverrideEntryPointIdForTesting(const std::string& test_entry_point_id);

  void SetGoogleChromeBuildForTesting();

 private:
  // Gets `web_app::WebAppRegistrar` for querying information about new entry
  // point. Use a pointer instead of a reference as `base::expected` is
  // incompatible with a reference.
  base::expected<const web_app::WebAppRegistrar*,
                 ash::assistant::AssistantBrowserDelegate::Error>
  GetWebAppRegistrarForNewEntryPoint();

  // Resolves new entry point if a device or a profile is eligible. Note that
  // it's guaranteed that the value is non-nullptr if provided.
  base::expected<const web_app::WebApp*,
                 ash::assistant::AssistantBrowserDelegate::Error>
  ResolveNewEntryPointIfEligible();

  void OnExternalManagersSynchronized();

  void MaybeInit(Profile* profile);
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

  // ash::AssistantStateObserver:
  void OnAssistantFeatureAllowedChanged(
      ash::assistant::AssistantAllowedState allowed_state) override;

  // Called when the application is terminating
  void OnAppTerminating();

  // Initializes new entry point for a passed primary profile. Note that
  // Assistant new entry point is eligible only for a primary profile.
  void InitializeNewEntryPointFor(Profile* primary_profile);

  std::unique_ptr<DeviceActions> device_actions_;
  std::unique_ptr<AssistantSetup> assistant_setup_;

  bool initialized_ = false;

  base::CallbackListSubscription subscription_;

  // Non-owning pointers.
  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  // Stores a profile for Assistant new entry point. Note that
  // `AssistantBrowserDelegateImpl::profile_` is only initialized when Assistant
  // is allowed.
  raw_ptr<Profile> profile_for_new_entry_point_ = nullptr;

  std::string entry_point_id_for_testing_;

  bool is_google_chrome_override_for_testing_ = false;

  base::ScopedObservation<ash::AssistantStateBase, ash::AssistantStateObserver>
      assistant_state_observation_{this};

  base::WeakPtrFactory<AssistantBrowserDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_BROWSER_DELEGATE_IMPL_H_
