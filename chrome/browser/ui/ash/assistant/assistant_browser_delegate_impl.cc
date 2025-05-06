// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_browser_delegate_impl.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/assistant/assistant_interface_binder.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/assistant/assistant_setup.h"
#include "chrome/browser/ui/ash/assistant/device_actions_delegate_impl.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/ash/services/libassistant/public/mojom/service.mojom.h"
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)

namespace {

using AssistantBrowserDelegate = ash::assistant::AssistantBrowserDelegate;

Profile* GetActiveUserProfile() {
  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  CHECK(active_user);

  return ash::ProfileHelper::Get()->GetProfileByUser(active_user);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If any value is added, please update
// `AssistantNewEntryPointEligibility` in `enums.xml`
// LINT.IfChange(AssistantNewEntryPointEligibility)
enum class AssistantNewEntryPointEligibility {
  // A profile is eligible for the new entry point.
  kEligible = 0,
  // A profile is not ready for eligibility check. This can be both permanent
  // error (e.g., guest session) and transient error (e.g., queried before user
  // session started).
  kErrorProfileNotReady = 1,
  // Web provider is not ready for eligibility check. The eligibility check
  // waits for external managers synchornization of `WebAppProvider`.
  kErrorWebProviderNotReady = 2,
  // Not eligible because the new entry point flag is off.
  kNotEligibleFlagOff = 3,
  // Not eligible because the new entry point is not installed.
  kNotEligibleNotInstalled = 4,
  // Not eligible because the build is not Google Chrome.
  kNotEligibleNonGoogleChrome = 5,
  kMaxValue = kNotEligibleNonGoogleChrome,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:AssistantNewEntryPointEligibility)

AssistantNewEntryPointEligibility ToHistogramEnum(
    base::expected<const web_app::WebApp*, AssistantBrowserDelegate::Error>
        maybe_web_app) {
  if (maybe_web_app.has_value()) {
    return AssistantNewEntryPointEligibility::kEligible;
  }

  switch (maybe_web_app.error()) {
    case AssistantBrowserDelegate::Error::kProfileNotReady:
      return AssistantNewEntryPointEligibility::kErrorProfileNotReady;
    case AssistantBrowserDelegate::Error::kWebAppProviderNotReadyToRead:
      return AssistantNewEntryPointEligibility::kErrorWebProviderNotReady;
    case AssistantBrowserDelegate::Error::kNewEntryPointNotEnabled:
      return AssistantNewEntryPointEligibility::kNotEligibleFlagOff;
    case AssistantBrowserDelegate::Error::kNewEntryPointNotFound:
      return AssistantNewEntryPointEligibility::kNotEligibleNotInstalled;
    case AssistantBrowserDelegate::Error::kNonGoogleChromeBuild:
      return AssistantNewEntryPointEligibility::kNotEligibleNonGoogleChrome;
  }

  CHECK(false) << "Invalid error value is specified";
}

base::expected<bool, AssistantBrowserDelegate::Error> ToEligibilityBool(
    base::expected<const web_app::WebApp*, AssistantBrowserDelegate::Error>
        maybe_web_app) {
  if (maybe_web_app.has_value()) {
    return true;
  }

  static constexpr auto non_transient_error =
      base::MakeFixedFlatSet<AssistantBrowserDelegate::Error>(
          {AssistantBrowserDelegate::Error::kNewEntryPointNotEnabled,
           AssistantBrowserDelegate::Error::kNewEntryPointNotFound,
           AssistantBrowserDelegate::Error::kNonGoogleChromeBuild});
  if (non_transient_error.contains(maybe_web_app.error())) {
    return false;
  }

  return base::unexpected(maybe_web_app.error());
}

bool IsGoogleChrome() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}

}  // namespace

AssistantBrowserDelegateImpl::AssistantBrowserDelegateImpl() {
  auto* session_manager = session_manager::SessionManager::Get();
  // AssistantBrowserDelegateImpl must be created before any user session is
  // created. Otherwise, it will not get OnUserProfileLoaded notification.
  DCHECK(session_manager->sessions().empty());
  session_manager->AddObserver(this);

  subscription_ = browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
      &AssistantBrowserDelegateImpl::OnAppTerminating, base::Unretained(this)));
}

AssistantBrowserDelegateImpl::~AssistantBrowserDelegateImpl() {
  session_manager::SessionManager::Get()->RemoveObserver(this);
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
  }
}

void AssistantBrowserDelegateImpl::MaybeInit(Profile* profile) {
  if (assistant::IsAssistantAllowedForProfile(profile) !=
      ash::assistant::AssistantAllowedState::ALLOWED) {
    return;
  }

  if (!profile_) {
    profile_ = profile;
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
    DCHECK(identity_manager_);
    identity_manager_->AddObserver(this);
  }
  DCHECK_EQ(profile_, profile);

  if (initialized_) {
    return;
  }

  initialized_ = true;

  device_actions_ = std::make_unique<DeviceActions>(
      std::make_unique<DeviceActionsDelegateImpl>());

  assistant_setup_ = std::make_unique<AssistantSetup>();
}

void AssistantBrowserDelegateImpl::MaybeStartAssistantOptInFlow() {
  if (!initialized_) {
    return;
  }

  assistant_setup_->MaybeStartAssistantOptInFlow();
}

void AssistantBrowserDelegateImpl::OnAppTerminating() {
  if (!initialized_) {
    return;
  }
}

void AssistantBrowserDelegateImpl::InitializeNewEntryPointFor(
    Profile* profile) {
  CHECK(profile);

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider) {
    // `WebAppProvider` is not available if `GetBrowserContextForWebApps` in
    // `web_app_utils.cc` returns nullptr, e.g., guest session. This is
    // non-recoverable, i.e., no need to wait and/or re-try.
    return;
  }

  if (profile_for_new_entry_point_) {
    CHECK_EQ(profile_for_new_entry_point_, profile)
        << "profile_for_new_entry_point_ is already initialized with a "
           "different profile. There should be only a single primary profile.";
    return;
  }

  // Profile is set only if `WebAppProvider` is available for the profile.
  profile_for_new_entry_point_ = profile;

  // Assistant new entry point is loaded to `WebAppProvider` as an async
  // operation. We have to wait for the async load before checking if Assistant
  // new entry point is installed on a device/profile.
  provider->on_external_managers_synchronized().Post(
      FROM_HERE,
      base::BindOnce(
          &AssistantBrowserDelegateImpl::OnExternalManagersSynchronized,
          weak_ptr_factory_.GetWeakPtr()));
}

void AssistantBrowserDelegateImpl::OnAssistantStatusChanged(
    ash::assistant::AssistantStatus new_status) {
  ash::AssistantState::Get()->NotifyStatusChanged(new_status);
}

void AssistantBrowserDelegateImpl::RequestAssistantVolumeControl(
    mojo::PendingReceiver<ash::mojom::AssistantVolumeControl> receiver) {
  ash::AssistantInterfaceBinder::GetInstance()->BindVolumeControl(
      std::move(receiver));
}

void AssistantBrowserDelegateImpl::RequestBatteryMonitor(
    mojo::PendingReceiver<device::mojom::BatteryMonitor> receiver) {
  content::GetDeviceService().BindBatteryMonitor(std::move(receiver));
}

void AssistantBrowserDelegateImpl::RequestWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  content::GetDeviceService().BindWakeLockProvider(std::move(receiver));
}

void AssistantBrowserDelegateImpl::RequestAudioStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
  content::GetAudioService().BindStreamFactory(std::move(receiver));
}

void AssistantBrowserDelegateImpl::RequestAudioDecoderFactory(
    mojo::PendingReceiver<ash::assistant::mojom::AssistantAudioDecoderFactory>
        receiver) {
  content::ServiceProcessHost::Launch(
      std::move(receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Assistant Audio Decoder Service")
          .Pass());
}

void AssistantBrowserDelegateImpl::RequestAudioFocusManager(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver) {
  content::GetMediaSessionService().BindAudioFocusManager(std::move(receiver));
}

void AssistantBrowserDelegateImpl::RequestMediaControllerManager(
    mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
        receiver) {
  content::GetMediaSessionService().BindMediaControllerManager(
      std::move(receiver));
}

void AssistantBrowserDelegateImpl::RequestNetworkConfig(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  ash::GetNetworkConfigService(std::move(receiver));
}

void AssistantBrowserDelegateImpl::OpenUrl(GURL url) {
  // The new tab should be opened with a user activation since the user
  // interacted with the Assistant to open the url. |in_background| describes
  // the relationship between |url| and Assistant UI, not the browser. As
  // such, the browser will always be instructed to open |url| in a new
  // browser tab and Assistant UI state will be updated downstream to respect
  // |in_background|.
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

base::expected<const web_app::WebAppRegistrar*, AssistantBrowserDelegate::Error>
AssistantBrowserDelegateImpl::GetWebAppRegistrarForNewEntryPoint() {
  if (!profile_for_new_entry_point_) {
    return base::unexpected(AssistantBrowserDelegate::Error::kProfileNotReady);
  }

  if (!on_is_new_entry_point_eligible_ready_.is_signaled()) {
    return base::unexpected(
        AssistantBrowserDelegate::Error::kWebAppProviderNotReadyToRead);
  }

  if (!ash::assistant::features::IsNewEntryPointEnabled()) {
    return base::unexpected(
        AssistantBrowserDelegate::Error::kNewEntryPointNotEnabled);
  }

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile_for_new_entry_point_);
  CHECK(provider) << "WebAppProvider must be available if "
                     "on_is_new_entry_point_eligible_ready_ is signaled";

  return &(provider->registrar_unsafe());
}

base::expected<const web_app::WebApp*, AssistantBrowserDelegate::Error>
AssistantBrowserDelegateImpl::ResolveNewEntryPointIfEligible() {
  if (!IsGoogleChrome() && !is_google_chrome_override_for_testing_) {
    return base::unexpected(
        AssistantBrowserDelegate::Error::kNonGoogleChromeBuild);
  }

  ASSIGN_OR_RETURN(const web_app::WebAppRegistrar* web_app_registrar,
                   GetWebAppRegistrarForNewEntryPoint());

  std::string app_id = entry_point_id_for_testing_.empty()
                           ? ash::kGeminiAppId
                           : entry_point_id_for_testing_;
  const web_app::WebApp* web_app = web_app_registrar->GetAppById(app_id);
  if (!web_app) {
    return base::unexpected(
        AssistantBrowserDelegate::Error::kNewEntryPointNotFound);
  }

  return web_app;
}

void AssistantBrowserDelegateImpl::OnExternalManagersSynchronized() {
  on_is_new_entry_point_eligible_ready_.Signal();
}

base::expected<bool, AssistantBrowserDelegate::Error>
AssistantBrowserDelegateImpl::IsNewEntryPointEligibleForPrimaryProfile() {
  base::expected<const web_app::WebApp*, AssistantBrowserDelegate::Error>
      maybe_web_app = ResolveNewEntryPointIfEligible();

  base::UmaHistogramEnumeration("Assistant.NewEntryPoint.Eligibility",
                                ToHistogramEnum(maybe_web_app));
  return ToEligibilityBool(maybe_web_app);
}

void AssistantBrowserDelegateImpl::OpenNewEntryPoint() {
  ASSIGN_OR_RETURN(const web_app::WebApp* web_app,
                   ResolveNewEntryPointIfEligible(), [](auto) {});
  CHECK(profile_for_new_entry_point_);

  // Check if the app is already running. If it is, bring the window to front.
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if ((browser->is_type_app() || browser->is_type_app_popup()) &&
        web_app->app_id() ==
            web_app::GetAppIdFromApplicationName(browser->app_name()) &&
        profile_for_new_entry_point_ == browser->profile()) {
      browser->window()->Show();
      return;
    }
  }

  apps::AppServiceProxyFactory::GetForProfile(profile_for_new_entry_point_)
      ->LaunchAppWithParams(apps::AppLaunchParams(
          web_app->app_id(), apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW,
          // TODO(xiaohuic): maybe add new source
          apps::LaunchSource::kUnknown));
}

std::optional<std::string>
AssistantBrowserDelegateImpl::GetNewEntryPointName() {
  ASSIGN_OR_RETURN(
      const web_app::WebApp* web_app, ResolveNewEntryPointIfEligible(),
      [](auto) -> std::optional<std::string> { return std::nullopt; });
  ASSIGN_OR_RETURN(
      const web_app::WebAppRegistrar* web_app_registrar,
      GetWebAppRegistrarForNewEntryPoint(),
      [](auto) -> std::optional<std::string> { return std::nullopt; });

  return web_app_registrar->GetAppShortName(web_app->app_id());
}

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
void AssistantBrowserDelegateImpl::RequestLibassistantService(
    mojo::PendingReceiver<ash::libassistant::mojom::LibassistantService>
        receiver) {
  content::ServiceProcessHost::Launch<
      ash::libassistant::mojom::LibassistantService>(
      std::move(receiver), content::ServiceProcessHost::Options()
                               .WithDisplayName("Libassistant Service")
                               .Pass());
}
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)

void AssistantBrowserDelegateImpl::OverrideEntryPointIdForTesting(
    const std::string& test_entry_point_id) {
  CHECK_IS_TEST();
  entry_point_id_for_testing_ = test_entry_point_id;
}

void AssistantBrowserDelegateImpl::SetGoogleChromeBuildForTesting() {
  CHECK_IS_TEST();
  CHECK(!is_google_chrome_override_for_testing_)
      << "Already marked as google chrome";
  is_google_chrome_override_for_testing_ = true;
}

void AssistantBrowserDelegateImpl::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (initialized_) {
    return;
  }

  MaybeInit(profile_);
}

void AssistantBrowserDelegateImpl::OnUserProfileLoaded(
    const AccountId& account_id) {
  if (!assistant_state_observation_.IsObserving() && !initialized_ &&
      ash::AssistantState::Get()) {
    assistant_state_observation_.Observe(ash::AssistantState::Get());
  }
}

void AssistantBrowserDelegateImpl::OnUserSessionStarted(bool is_primary_user) {
  if (is_primary_user) {
    InitializeNewEntryPointFor(GetActiveUserProfile());
  }

  if (ash::features::IsOobeSkipAssistantEnabled()) {
    return;
  }

  // Disable the handling for browser tests to prevent the Assistant being
  // enabled unexpectedly.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (is_primary_user && !ash::switches::ShouldSkipOobePostLogin() &&
      !command_line->HasSwitch(switches::kBrowserTest)) {
    MaybeStartAssistantOptInFlow();
  }
}

void AssistantBrowserDelegateImpl::OnAssistantFeatureAllowedChanged(
    ash::assistant::AssistantAllowedState allowed_state) {
  Profile* profile = ProfileManager::GetActiveUserProfile();

  MaybeInit(profile);
}
