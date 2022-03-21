// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/lacros_service.h"

#include <atomic>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/crosapi/mojom/app_window_tracker.mojom.h"
#include "chromeos/crosapi/mojom/arc.mojom.h"
#include "chromeos/crosapi/mojom/authentication.mojom.h"
#include "chromeos/crosapi/mojom/automation.mojom.h"
#include "chromeos/crosapi/mojom/browser_app_instance_registry.mojom.h"
#include "chromeos/crosapi/mojom/browser_version.mojom.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/crosapi/mojom/clipboard.mojom.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/crosapi/mojom/content_protection.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/desk_template.mojom.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/crosapi/mojom/dlp.mojom.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/crosapi/mojom/field_trial.mojom.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/crosapi/mojom/force_installed_tracker.mojom.h"
#include "chromeos/crosapi/mojom/geolocation.mojom.h"
#include "chromeos/crosapi/mojom/holding_space_service.mojom.h"
#include "chromeos/crosapi/mojom/identity_manager.mojom.h"
#include "chromeos/crosapi/mojom/image_writer.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/kiosk_session_service.mojom.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/crosapi/mojom/login.mojom.h"
#include "chromeos/crosapi/mojom/login_screen_storage.mojom.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "chromeos/crosapi/mojom/networking_attributes.mojom.h"
#include "chromeos/crosapi/mojom/policy_service.mojom.h"
#include "chromeos/crosapi/mojom/power.mojom.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/crosapi/mojom/remoting.mojom.h"
#include "chromeos/crosapi/mojom/resource_manager.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/select_file.mojom.h"
#include "chromeos/crosapi/mojom/sharesheet.mojom.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "chromeos/crosapi/mojom/system_display.mojom.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/crosapi/mojom/timezone.mojom.h"
#include "chromeos/crosapi/mojom/tts.mojom.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "chromeos/crosapi/mojom/web_page_info.mojom.h"
#include "chromeos/lacros/lacros_service_never_blocking_state.h"
#include "chromeos/lacros/native_theme_cache.h"
#include "chromeos/lacros/system_idle_cache.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/startup/startup.h"
#include "components/crash/core/common/crash_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

using Crosapi = crosapi::mojom::Crosapi;

// We use a std::atomic here rather than a base::NoDestructor because we want to
// allow instances of LacrosService to be destroyed to facilitate
// testing.
std::atomic<LacrosService*> g_instance = {nullptr};

crosapi::mojom::BrowserInfoPtr ToMojo(const std::string& browser_version) {
  auto info = crosapi::mojom::BrowserInfo::New();
  info->browser_version = browser_version;
  return info;
}

// Reads and parses the startup data to BrowserInitParams.
// If data is missing, or failed to parse, returns a null StructPtr.
crosapi::mojom::BrowserInitParamsPtr ReadStartupBrowserInitParams() {
  absl::optional<std::string> content = ReadStartupData();
  if (!content)
    return {};

  crosapi::mojom::BrowserInitParamsPtr result;
  if (!crosapi::mojom::BrowserInitParams::Deserialize(
          content->data(), content->size(), &result)) {
    LOG(ERROR) << "Failed to parse startup data";
    return {};
  }

  return result;
}

std::string SessionTypeToString(crosapi::mojom::SessionType session_type) {
  switch (session_type) {
    case crosapi::mojom::SessionType::kUnknown:
      return "unknown";
    case crosapi::mojom::SessionType::kRegularSession:
      return "regular";
    case crosapi::mojom::SessionType::kGuestSession:
      return "guest";
    case crosapi::mojom::SessionType::kPublicSession:
      return "managed-guest-session";
    case crosapi::mojom::SessionType::kWebKioskSession:
      return "web-kiosk";
    case crosapi::mojom::SessionType::kChildSession:
      return "child";
    case crosapi::mojom::SessionType::kAppKioskSession:
      return "chrome-app-kiosk";
  }
}

}  // namespace

LacrosService::InterfaceEntryBase::InterfaceEntryBase() = default;
LacrosService::InterfaceEntryBase::~InterfaceEntryBase() = default;

template <typename CrosapiInterface,
          void (Crosapi::*bind_func)(mojo::PendingReceiver<CrosapiInterface>),
          uint32_t MethodMinVersion>
class LacrosService::InterfaceEntry : public LacrosService::InterfaceEntryBase {
 public:
  InterfaceEntry() : InterfaceEntryBase() {}
  InterfaceEntry(const InterfaceEntry&) = delete;
  InterfaceEntry& operator=(const InterfaceEntry&) = delete;
  ~InterfaceEntry() override = default;
  void* GetInternal() override { return &remote_; }
  void MaybeBind(uint32_t crosapi_version, LacrosService* impl) override {
    available_ = crosapi_version >= MethodMinVersion;
    if (available_) {
      impl->InitializeAndBindRemote<CrosapiInterface, bind_func>(&remote_);
    }
  }

 private:
  mojo::Remote<CrosapiInterface> remote_;
};

// static
LacrosService* LacrosService::Get() {
  // If this returns null and causes failure in a unit test, consider using
  // ScopedLacrosServiceTestHelper in the test to instantiate
  // LacrosService.
  return g_instance;
}

LacrosService::LacrosService()
    :  // If crosapi is disabled, use the empty params.
       // Otherwise, read the startup data from the inherited FD.
      init_params_(disable_crosapi_for_testing_
                       ? crosapi::mojom::BrowserInitParams::New()
                       : ReadStartupBrowserInitParams()),
      never_blocking_sequence_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      sequenced_state_(new LacrosServiceNeverBlockingState(),
                       base::OnTaskRunnerDeleter(never_blocking_sequence_)),
      weak_sequenced_state_(sequenced_state_->GetWeakPtr()),
      observer_list_(
          base::MakeRefCounted<base::ObserverListThreadSafe<Observer>>()) {
  DCHECK(init_params_);
  if (init_params_->idle_info) {
    // Presence of initial |idle_info| indicates that ash-chrome can stream
    // idle info updates, so instantiate under Streaming mode, using
    // |idle_info| as initial cached values.
    system_idle_cache_ =
        std::make_unique<SystemIdleCache>(*init_params_->idle_info);

    // After construction finishes, start caching.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&LacrosService::StartSystemIdleCache,
                                  weak_factory_.GetWeakPtr()));
  } else {
    // Ash-chrome cannot stream, so instantiate under fallback mode.
    system_idle_cache_ = std::make_unique<SystemIdleCache>();
  }

  if (init_params_->native_theme_info) {
    // Start Lacros' native theme caching, since it is available in Ash.
    native_theme_cache_ =
        std::make_unique<NativeThemeCache>(*init_params_->native_theme_info);

    // After construction finishes, start caching.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&LacrosService::StartNativeThemeCache,
                                  weak_factory_.GetWeakPtr()));
  }

  static crash_reporter::CrashKeyString<32> session_type("session-type");
  session_type.Set(SessionTypeToString(init_params_->session_type));

  // Short term workaround: if --crosapi-mojo-platform-channel-handle is
  // available, close --mojo-platform-channel-handle, and remove it
  // from command line. It is for backward compatibility support by
  // ash-chrome.
  // TODO(crbug.com/1180712): Remove this, when ash-chrome stops to support
  // legacy invitation flow.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(crosapi::kCrosapiMojoPlatformChannelHandle) &&
      command_line->HasSwitch(mojo::PlatformChannel::kHandleSwitch)) {
    std::ignore = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
        *command_line);
    command_line->RemoveSwitch(mojo::PlatformChannel::kHandleSwitch);
  }

  never_blocking_sequence_->PostTask(
      FROM_HERE, base::BindOnce(&LacrosServiceNeverBlockingState::BindCrosapi,
                                weak_sequenced_state_));

  // Note: sorted by the Bind method names in the lexicographical order.
  ConstructRemote<crosapi::mojom::Arc, &Crosapi::BindArc,
                  Crosapi::MethodMinVersions::kBindArcMinVersion>();
  ConstructRemote<
      crosapi::mojom::AutomationFactory, &Crosapi::BindAutomationFactory,
      Crosapi::MethodMinVersions::kBindAutomationFactoryMinVersion>();
  ConstructRemote<crosapi::mojom::AppServiceProxy,
                  &Crosapi::BindAppServiceProxy,
                  Crosapi::MethodMinVersions::kBindAppServiceProxyMinVersion>();
  ConstructRemote<crosapi::mojom::Authentication, &Crosapi::BindAuthentication,
                  Crosapi::MethodMinVersions::kBindAuthenticationMinVersion>();
  ConstructRemote<
      crosapi::mojom::AppWindowTracker, &Crosapi::BindChromeAppWindowTracker,
      Crosapi::MethodMinVersions::kBindChromeAppWindowTrackerMinVersion>();
  ConstructRemote<
      crosapi::mojom::BrowserAppInstanceRegistry,
      &Crosapi::BindBrowserAppInstanceRegistry,
      Crosapi::MethodMinVersions::kBindBrowserAppInstanceRegistryMinVersion>();
  ConstructRemote<
      crosapi::mojom::BrowserServiceHost, &Crosapi::BindBrowserServiceHost,
      Crosapi::MethodMinVersions::kBindBrowserServiceHostMinVersion>();
  ConstructRemote<
      crosapi::mojom::BrowserVersionService,
      &crosapi::mojom::Crosapi::BindBrowserVersionService,
      Crosapi::MethodMinVersions::kBindBrowserVersionServiceMinVersion>();
  ConstructRemote<crosapi::mojom::CertDatabase, &Crosapi::BindCertDatabase,
                  Crosapi::MethodMinVersions::kBindCertDatabaseMinVersion>();
  ConstructRemote<crosapi::mojom::Clipboard, &Crosapi::BindClipboard,
                  Crosapi::MethodMinVersions::kBindClipboardMinVersion>();
  ConstructRemote<
      crosapi::mojom::ClipboardHistory, &Crosapi::BindClipboardHistory,
      Crosapi::MethodMinVersions::kBindClipboardHistoryMinVersion>();
  ConstructRemote<
      crosapi::mojom::ContentProtection, &Crosapi::BindContentProtection,
      Crosapi::MethodMinVersions::kBindContentProtectionMinVersion>();
  ConstructRemote<crosapi::mojom::DeskTemplate, &Crosapi::BindDeskTemplate,
                  Crosapi::MethodMinVersions::kBindDeskTemplateMinVersion>();
  ConstructRemote<
      crosapi::mojom::DeviceAttributes, &Crosapi::BindDeviceAttributes,
      Crosapi::MethodMinVersions::kBindDeviceAttributesMinVersion>();
  ConstructRemote<
      crosapi::mojom::DeviceSettingsService,
      &Crosapi::BindDeviceSettingsService,
      Crosapi::MethodMinVersions::kBindDeviceSettingsServiceMinVersion>();
  ConstructRemote<crosapi::mojom::Dlp, &Crosapi::BindDlp,
                  Crosapi::MethodMinVersions::kBindDlpMinVersion>();
  ConstructRemote<
      crosapi::mojom::DownloadController, &Crosapi::BindDownloadController,
      Crosapi::MethodMinVersions::kBindDownloadControllerMinVersion>();
  ConstructRemote<
      crosapi::mojom::DriveIntegrationService,
      &crosapi::mojom::Crosapi::BindDriveIntegrationService,
      Crosapi::MethodMinVersions::kBindDriveIntegrationServiceMinVersion>();
  ConstructRemote<crosapi::mojom::Feedback,
                  &crosapi::mojom::Crosapi::BindFeedback,
                  Crosapi::MethodMinVersions::kBindFeedbackMinVersion>();
  ConstructRemote<crosapi::mojom::FileManager,
                  &crosapi::mojom::Crosapi::BindFileManager,
                  Crosapi::MethodMinVersions::kBindFileManagerMinVersion>();
  ConstructRemote<
      crosapi::mojom::FieldTrialService,
      &crosapi::mojom::Crosapi::BindFieldTrialService,
      Crosapi::MethodMinVersions::kBindFieldTrialServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::ForceInstalledTracker,
      &crosapi::mojom::Crosapi::BindForceInstalledTracker,
      Crosapi::MethodMinVersions::kBindForceInstalledTrackerMinVersion>();
  ConstructRemote<
      crosapi::mojom::GeolocationService,
      &crosapi::mojom::Crosapi::BindGeolocationService,
      Crosapi::MethodMinVersions::kBindGeolocationServiceMinVersion>();
  ConstructRemote<device::mojom::HidManager,
                  &crosapi::mojom::Crosapi::BindHidManager,
                  Crosapi::MethodMinVersions::kBindHidManagerMinVersion>();
  ConstructRemote<
      crosapi::mojom::HoldingSpaceService,
      &crosapi::mojom::Crosapi::BindHoldingSpaceService,
      Crosapi::MethodMinVersions::kBindHoldingSpaceServiceMinVersion>();
  ConstructRemote<crosapi::mojom::IdentityManager,
                  &crosapi::mojom::Crosapi::BindIdentityManager,
                  Crosapi::MethodMinVersions::kBindIdentityManagerMinVersion>();
  ConstructRemote<crosapi::mojom::IdleService,
                  &crosapi::mojom::Crosapi::BindIdleService,
                  Crosapi::MethodMinVersions::kBindIdleServiceMinVersion>();
  ConstructRemote<crosapi::mojom::ImageWriter,
                  &crosapi::mojom::Crosapi::BindImageWriter,
                  Crosapi::MethodMinVersions::kBindImageWriterMinVersion>();
  ConstructRemote<crosapi::mojom::KeystoreService,
                  &crosapi::mojom::Crosapi::BindKeystoreService,
                  Crosapi::MethodMinVersions::kBindKeystoreServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::KioskSessionService, &Crosapi::BindKioskSessionService,
      Crosapi::MethodMinVersions::kBindKioskSessionServiceMinVersion>();
  ConstructRemote<crosapi::mojom::LocalPrinter,
                  &crosapi::mojom::Crosapi::BindLocalPrinter,
                  Crosapi::MethodMinVersions::kBindLocalPrinterMinVersion>();
  ConstructRemote<crosapi::mojom::Login, &crosapi::mojom::Crosapi::BindLogin,
                  Crosapi::MethodMinVersions::kBindLoginMinVersion>();
  ConstructRemote<
      crosapi::mojom::LoginScreenStorage,
      &crosapi::mojom::Crosapi::BindLoginScreenStorage,
      Crosapi::MethodMinVersions::kBindLoginScreenStorageMinVersion>();
  ConstructRemote<crosapi::mojom::LoginState,
                  &crosapi::mojom::Crosapi::BindLoginState,
                  Crosapi::MethodMinVersions::kBindLoginStateMinVersion>();
  ConstructRemote<
      chromeos::machine_learning::mojom::MachineLearningService,
      &crosapi::mojom::Crosapi::BindMachineLearningService,
      Crosapi::MethodMinVersions::kBindMachineLearningServiceMinVersion>();
  ConstructRemote<crosapi::mojom::MessageCenter,
                  &crosapi::mojom::Crosapi::BindMessageCenter,
                  Crosapi::MethodMinVersions::kBindMessageCenterMinVersion>();
  ConstructRemote<
      crosapi::mojom::NativeThemeService,
      &crosapi::mojom::Crosapi::BindNativeThemeService,
      Crosapi::MethodMinVersions::kBindNativeThemeServiceMinVersion>();
  ConstructRemote<crosapi::mojom::Power, &crosapi::mojom::Crosapi::BindPower,
                  Crosapi::MethodMinVersions::kBindPowerMinVersion>();
  ConstructRemote<
      crosapi::mojom::NetworkingAttributes, &Crosapi::BindNetworkingAttributes,
      Crosapi::MethodMinVersions::kBindNetworkingAttributesMinVersion>();
  ConstructRemote<crosapi::mojom::Prefs, &crosapi::mojom::Crosapi::BindPrefs,
                  Crosapi::MethodMinVersions::kBindPrefsMinVersion>();
  ConstructRemote<
      crosapi::mojom::NetworkSettingsService,
      &crosapi::mojom::Crosapi::BindNetworkSettingsService,
      Crosapi::MethodMinVersions::kBindNetworkSettingsServiceMinVersion>();
  ConstructRemote<crosapi::mojom::PolicyService, &Crosapi::BindPolicyService,
                  Crosapi::MethodMinVersions::kBindPolicyServiceMinVersion>();
  ConstructRemote<crosapi::mojom::Remoting,
                  &crosapi::mojom::Crosapi::BindRemoting,
                  Crosapi::MethodMinVersions::kBindRemotingMinVersion>();
  ConstructRemote<crosapi::mojom::ResourceManager,
                  &crosapi::mojom::Crosapi::BindResourceManager,
                  Crosapi::MethodMinVersions::kBindResourceManagerMinVersion>();
  ConstructRemote<crosapi::mojom::SelectFile,
                  &crosapi::mojom::Crosapi::BindSelectFile,
                  Crosapi::MethodMinVersions::kBindSelectFileMinVersion>();
  ConstructRemote<
      crosapi::mojom::SearchControllerRegistry,
      &crosapi::mojom::Crosapi::BindSearchControllerRegistry,
      Crosapi::MethodMinVersions::kBindSearchControllerRegistryMinVersion>();
  ConstructRemote<crosapi::mojom::Sharesheet,
                  &crosapi::mojom::Crosapi::BindSharesheet,
                  Crosapi::MethodMinVersions::kBindSharesheetMinVersion>();
  ConstructRemote<
      crosapi::mojom::StructuredMetricsService,
      &crosapi::mojom::Crosapi::BindStructuredMetricsService,
      Crosapi::MethodMinVersions::kBindStructuredMetricsServiceMinVersion>();
  ConstructRemote<crosapi::mojom::SyncService,
                  &crosapi::mojom::Crosapi::BindSyncService,
                  Crosapi::MethodMinVersions::kBindSyncServiceMinVersion>();
  ConstructRemote<crosapi::mojom::SystemDisplay, &Crosapi::BindSystemDisplay,
                  Crosapi::MethodMinVersions::kBindSystemDisplayMinVersion>();
  ConstructRemote<crosapi::mojom::TaskManager,
                  &crosapi::mojom::Crosapi::BindTaskManager,
                  Crosapi::MethodMinVersions::kBindTaskManagerMinVersion>();
  ConstructRemote<crosapi::mojom::TimeZoneService,
                  &crosapi::mojom::Crosapi::BindTimeZoneService,
                  Crosapi::MethodMinVersions::kBindTimeZoneServiceMinVersion>();
  ConstructRemote<crosapi::mojom::Tts, &crosapi::mojom::Crosapi::BindTts,
                  Crosapi::MethodMinVersions::kBindTtsMinVersion>();
  ConstructRemote<crosapi::mojom::UrlHandler,
                  &crosapi::mojom::Crosapi::BindUrlHandler,
                  Crosapi::MethodMinVersions::kBindUrlHandlerMinVersion>();
  ConstructRemote<crosapi::mojom::AppPublisher, &Crosapi::BindWebAppPublisher,
                  Crosapi::MethodMinVersions::kBindWebAppPublisherMinVersion>();
  ConstructRemote<crosapi::mojom::WebAppService,
                  &crosapi::mojom::Crosapi::BindWebAppService,
                  Crosapi::MethodMinVersions::kBindWebAppServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::WebPageInfoFactory,
      &crosapi::mojom::Crosapi::BindWebPageInfoFactory,
      Crosapi::MethodMinVersions::kBindWebPageInfoFactoryMinVersion>();

#if !BUILDFLAG(IS_CHROMEOS_DEVICE)
  // The test controller is not available on production devices as tests only
  // run on Linux.
  ConstructRemote<crosapi::mojom::TestController,
                  &crosapi::mojom::Crosapi::BindTestController,
                  Crosapi::MethodMinVersions::kBindTestControllerMinVersion>();
#endif

  DCHECK(!g_instance);
  g_instance = this;
}

LacrosService::~LacrosService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void LacrosService::BindReceiver(const std::string& browser_version) {
  // Accept Crosapi invitation here. Mojo IPC support should be initialized
  // at this stage.
  auto* command_line = base::CommandLine::ForCurrentProcess();

  // In unittests/browser_tests cases, the mojo pipe may not be set up.
  // Just ignore the case.
  if (!command_line->HasSwitch(crosapi::kCrosapiMojoPlatformChannelHandle))
    return;

  mojo::PlatformChannelEndpoint endpoint =
      mojo::PlatformChannel::RecoverPassedEndpointFromString(
          command_line->GetSwitchValueASCII(
              crosapi::kCrosapiMojoPlatformChannelHandle));
  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  never_blocking_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&LacrosServiceNeverBlockingState::FusePipeCrosapi,
                     weak_sequenced_state_,
                     mojo::PendingRemote<crosapi::mojom::Crosapi>(
                         invitation.ExtractMessagePipe(0), /*version=*/0)));

  did_bind_receiver_ = true;

  if (CrosapiVersion()) {
    for (auto& entry : interfaces_) {
      entry.second->MaybeBind(*CrosapiVersion(), this);
    }
  }

  if (IsOnBrowserStartupAvailable()) {
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&LacrosServiceNeverBlockingState::OnBrowserStartup,
                       weak_sequenced_state_, ToMojo(browser_version)));
  }
}

bool LacrosService::IsAccountManagerAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindAccountManagerMinVersion;
}

bool LacrosService::IsBrowserCdmFactoryAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindBrowserCdmFactoryMinVersion;
}

bool LacrosService::IsMediaSessionAudioFocusAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindMediaSessionAudioFocusMinVersion;
}

bool LacrosService::IsMediaSessionAudioFocusDebugAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::
                            kBindMediaSessionAudioFocusDebugMinVersion;
}

bool LacrosService::IsMediaSessionControllerAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindMediaSessionControllerMinVersion;
}

bool LacrosService::IsMetricsReportingAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindMetricsReportingMinVersion;
}

bool LacrosService::IsScreenManagerAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindScreenManagerMinVersion;
}

bool LacrosService::IsSensorHalClientAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version &&
         version.value() >=
             Crosapi::MethodMinVersions::kBindSensorHalClientMinVersion;
}

void LacrosService::BindAccountManagerReceiver(
    mojo::PendingReceiver<crosapi::mojom::AccountManager> pending_receiver) {
  DCHECK(IsAccountManagerAvailable());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::AccountManager>,
      &crosapi::mojom::Crosapi::BindAccountManager>(
      std::move(pending_receiver));
}

void LacrosService::BindAudioFocusManager(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManager> remote) {
  DCHECK(IsMediaSessionAudioFocusAvailable());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager>,
      &crosapi::mojom::Crosapi::BindMediaSessionAudioFocus>(std::move(remote));
}

void LacrosService::BindAudioFocusManagerDebug(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
        remote) {
  DCHECK(IsMediaSessionAudioFocusAvailable());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>,
      &crosapi::mojom::Crosapi::BindMediaSessionAudioFocusDebug>(
      std::move(remote));
}

void LacrosService::BindBrowserCdmFactory(
    mojo::GenericPendingReceiver receiver) {
  DCHECK(IsBrowserCdmFactoryAvailable());
  BindPendingReceiverOrRemote<mojo::GenericPendingReceiver,
                              &crosapi::mojom::Crosapi::BindBrowserCdmFactory>(
      std::move(receiver));
}

void LacrosService::BindGeolocationService(
    mojo::PendingReceiver<crosapi::mojom::GeolocationService>
        pending_receiver) {
  DCHECK(IsAvailable<crosapi::mojom::GeolocationService>());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::GeolocationService>,
      &crosapi::mojom::Crosapi::BindGeolocationService>(
      std::move(pending_receiver));
}

void LacrosService::BindMachineLearningService(
    mojo::PendingReceiver<
        chromeos::machine_learning::mojom::MachineLearningService> receiver) {
  DCHECK(
      IsAvailable<chromeos::machine_learning::mojom::MachineLearningService>());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<
          chromeos::machine_learning::mojom::MachineLearningService>,
      &crosapi::mojom::Crosapi::BindMachineLearningService>(
      std::move(receiver));
}

void LacrosService::BindMediaControllerManager(
    mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
        remote) {
  DCHECK(IsMediaSessionAudioFocusAvailable());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>,
      &crosapi::mojom::Crosapi::BindMediaSessionController>(std::move(remote));
}

void LacrosService::BindMetricsReporting(
    mojo::PendingReceiver<crosapi::mojom::MetricsReporting> receiver) {
  DCHECK(IsMetricsReportingAvailable());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::MetricsReporting>,
      &crosapi::mojom::Crosapi::BindMetricsReporting>(std::move(receiver));
}

void LacrosService::BindScreenManagerReceiver(
    mojo::PendingReceiver<crosapi::mojom::ScreenManager> pending_receiver) {
  DCHECK(IsScreenManagerAvailable());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::ScreenManager>,
      &crosapi::mojom::Crosapi::BindScreenManager>(std::move(pending_receiver));
}

void LacrosService::BindSensorHalClient(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote) {
  DCHECK(IsSensorHalClientAvailable());
  BindPendingReceiverOrRemote<
      mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient>,
      &crosapi::mojom::Crosapi::BindSensorHalClient>(std::move(remote));
}

bool LacrosService::IsOnBrowserStartupAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::kOnBrowserStartupMinVersion;
}

void LacrosService::BindVideoCaptureDeviceFactory(
    mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory>
        pending_receiver) {
  DCHECK(IsVideoCaptureDeviceFactoryAvailable());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory>,
      &crosapi::mojom::Crosapi::BindVideoCaptureDeviceFactory>(
      std::move(pending_receiver));
}

bool LacrosService::IsVideoCaptureDeviceFactoryAvailable() const {
  absl::optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::
                            kBindVideoCaptureDeviceFactoryMinVersion;
}

int LacrosService::GetInterfaceVersion(base::Token interface_uuid) const {
  if (disable_crosapi_for_testing_)
    return -1;
  if (!init_params_->interface_versions)
    return -1;
  const base::flat_map<base::Token, uint32_t>& versions =
      init_params_->interface_versions.value();
  auto it = versions.find(interface_uuid);
  if (it == versions.end())
    return -1;
  return it->second;
}

void LacrosService::SetInitParamsForTests(
    crosapi::mojom::BrowserInitParamsPtr init_params) {
  init_params_ = std::move(init_params);
}

absl::optional<uint32_t> LacrosService::CrosapiVersion() const {
  if (disable_crosapi_for_testing_)
    return absl::nullopt;
  DCHECK(did_bind_receiver_);
  return init_params_->crosapi_version;
}

void LacrosService::StartSystemIdleCache() {
  system_idle_cache_->Start();
}

void LacrosService::StartNativeThemeCache() {
  native_theme_cache_->Start();
}

template <typename CrosapiInterface,
          void (Crosapi::*bind_func)(mojo::PendingReceiver<CrosapiInterface>)>
void LacrosService::InitializeAndBindRemote(
    mojo::Remote<CrosapiInterface>* remote) {
  mojo::PendingReceiver<CrosapiInterface> pending_receiver =
      remote->BindNewPipeAndPassReceiver();
  BindPendingReceiverOrRemote<mojo::PendingReceiver<CrosapiInterface>,
                              bind_func>(std::move(pending_receiver));
}

template <typename CrosapiInterface,
          void (Crosapi::*bind_func)(mojo::PendingReceiver<CrosapiInterface>),
          uint32_t MethodMinVersion>
void LacrosService::ConstructRemote() {
  DCHECK(!base::Contains(interfaces_, CrosapiInterface::Uuid_));
  interfaces_.emplace(CrosapiInterface::Uuid_,
                      std::make_unique<LacrosService::InterfaceEntry<
                          CrosapiInterface, bind_func, MethodMinVersion>>());
}

void LacrosService::AddObserver(Observer* obs) {
  observer_list_->AddObserver(obs);
}

void LacrosService::RemoveObserver(Observer* obs) {
  observer_list_->RemoveObserver(obs);
}

void LacrosService::NotifyPolicyUpdated(
    const std::vector<uint8_t>& policy_fetch_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  observer_list_->Notify(FROM_HERE, &Observer::OnPolicyUpdated,
                         policy_fetch_response);
}

// static
bool LacrosService::disable_crosapi_for_testing_ = false;

}  // namespace chromeos
