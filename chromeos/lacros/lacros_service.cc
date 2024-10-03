// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/lacros_service.h"

#include <atomic>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/components/cdm_factory_daemon/mojom/browser_cdm_factory.mojom.h"
#include "chromeos/components/in_session_auth/mojom/in_session_auth.mojom.h"
#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/crosapi/mojom/app_window_tracker.mojom.h"
#include "chromeos/crosapi/mojom/arc.mojom.h"
#include "chromeos/crosapi/mojom/audio_service.mojom.h"
#include "chromeos/crosapi/mojom/authentication.mojom.h"
#include "chromeos/crosapi/mojom/automation.mojom.h"
#include "chromeos/crosapi/mojom/browser_app_instance_registry.mojom.h"
#include "chromeos/crosapi/mojom/browser_service.mojom.h"
#include "chromeos/crosapi/mojom/browser_version.mojom.h"
#include "chromeos/crosapi/mojom/cec_private.mojom.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/crosapi/mojom/cert_provisioning.mojom.h"
#include "chromeos/crosapi/mojom/chaps_service.mojom.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"
#include "chromeos/crosapi/mojom/clipboard.mojom.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/crosapi/mojom/content_protection.mojom.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/debug_interface.mojom.h"
#include "chromeos/crosapi/mojom/desk.mojom.h"
#include "chromeos/crosapi/mojom/desk_profiles.mojom.h"
#include "chromeos/crosapi/mojom/desk_template.mojom.h"
#include "chromeos/crosapi/mojom/device_local_account_extension_service.mojom.h"
#include "chromeos/crosapi/mojom/device_oauth2_token_service.mojom.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/digital_goods.mojom.h"
#include "chromeos/crosapi/mojom/dlp.mojom.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "chromeos/crosapi/mojom/echo_private.mojom.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"
#include "chromeos/crosapi/mojom/embedded_accessibility_helper.mojom.h"
#include "chromeos/crosapi/mojom/emoji_picker.mojom.h"
#include "chromeos/crosapi/mojom/extension_info_private.mojom.h"
#include "chromeos/crosapi/mojom/extension_printer.mojom.h"
#include "chromeos/crosapi/mojom/eye_dropper.mojom.h"
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/crosapi/mojom/field_trial.mojom.h"
#include "chromeos/crosapi/mojom/file_change_service_bridge.mojom.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/crosapi/mojom/file_system_access_cloud_identifier.mojom.h"
#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"
#include "chromeos/crosapi/mojom/firewall_hole.mojom.h"
#include "chromeos/crosapi/mojom/force_installed_tracker.mojom.h"
#include "chromeos/crosapi/mojom/full_restore.mojom.h"
#include "chromeos/crosapi/mojom/fullscreen_controller.mojom.h"
#include "chromeos/crosapi/mojom/geolocation.mojom.h"
#include "chromeos/crosapi/mojom/guest_os_sk_forwarder.mojom.h"
#include "chromeos/crosapi/mojom/identity_manager.mojom.h"
#include "chromeos/crosapi/mojom/image_writer.mojom.h"
#include "chromeos/crosapi/mojom/input_methods.mojom.h"
#include "chromeos/crosapi/mojom/kerberos_in_browser.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/kiosk_session_service.mojom.h"
#include "chromeos/crosapi/mojom/lacros_shelf_item_tracker.mojom.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/crosapi/mojom/login.mojom.h"
#include "chromeos/crosapi/mojom/login_screen_storage.mojom.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "chromeos/crosapi/mojom/media_app.mojom.h"
#include "chromeos/crosapi/mojom/media_ui.mojom.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/metrics.mojom.h"
#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "chromeos/crosapi/mojom/multi_capture_service.mojom.h"
#include "chromeos/crosapi/mojom/network_change.mojom.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "chromeos/crosapi/mojom/networking_attributes.mojom.h"
#include "chromeos/crosapi/mojom/networking_private.mojom.h"
#include "chromeos/crosapi/mojom/nonclosable_app_toast_service.mojom.h"
#include "chromeos/crosapi/mojom/one_drive_integration_service.mojom.h"
#include "chromeos/crosapi/mojom/one_drive_notification_service.mojom.h"
#include "chromeos/crosapi/mojom/parent_access.mojom.h"
#include "chromeos/crosapi/mojom/policy_service.mojom.h"
#include "chromeos/crosapi/mojom/power.mojom.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom.h"
#include "chromeos/crosapi/mojom/printing_metrics.mojom.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/crosapi/mojom/remoting.mojom.h"
#include "chromeos/crosapi/mojom/resource_manager.mojom.h"
#include "chromeos/crosapi/mojom/screen_ai_downloader.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/sharesheet.mojom.h"
#include "chromeos/crosapi/mojom/smart_reader.mojom.h"
#include "chromeos/crosapi/mojom/speech_recognition.mojom.h"
#include "chromeos/crosapi/mojom/suggestion_service.mojom.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_management_service.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/crosapi/mojom/timezone.mojom.h"
#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "chromeos/crosapi/mojom/tts.mojom.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "chromeos/crosapi/mojom/virtual_keyboard.mojom.h"
#include "chromeos/crosapi/mojom/volume_manager.mojom.h"
#include "chromeos/crosapi/mojom/vpn_extension_observer.mojom.h"
#include "chromeos/crosapi/mojom/vpn_service.mojom.h"
#include "chromeos/crosapi/mojom/wallpaper.mojom.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom.h"
#include "chromeos/crosapi/mojom/web_page_info.mojom.h"
#include "chromeos/lacros/lacros_service_never_blocking_state.h"
#include "chromeos/lacros/native_theme_cache.h"
#include "chromeos/lacros/system_idle_cache.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/crash/core/common/crash_key.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
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
  void MaybeBind(LacrosService* impl) override {
    available_ = impl->IsSupported<CrosapiInterface>();
    if (available_) {
      if (!impl->MaybeInitializeAndBindRemote<CrosapiInterface, bind_func>(
              &remote_)) {
        LOG(ERROR) << "Failed to bind " << CrosapiInterface::Name_;
      }
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
    : never_blocking_sequence_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      sequenced_state_(new LacrosServiceNeverBlockingState(),
                       base::OnTaskRunnerDeleter(never_blocking_sequence_)),
      weak_sequenced_state_(sequenced_state_->GetWeakPtr()),
      observer_list_(
          base::MakeRefCounted<base::ObserverListThreadSafe<Observer>>()) {
  if (BrowserParamsProxy::Get()->IdleInfo()) {
    // Presence of initial |idle_info| indicates that ash-chrome can stream
    // idle info updates, so instantiate under Streaming mode, using
    // |idle_info| as initial cached values.
    system_idle_cache_ = std::make_unique<SystemIdleCache>(
        *BrowserParamsProxy::Get()->IdleInfo());

    // After construction finishes, start caching.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&LacrosService::StartSystemIdleCache,
                                  weak_factory_.GetWeakPtr()));
  } else {
    // Ash-chrome cannot stream, so instantiate under fallback mode.
    system_idle_cache_ = std::make_unique<SystemIdleCache>();
  }

  if (BrowserParamsProxy::Get()->NativeThemeInfo()) {
    // Start Lacros' native theme caching, since it is available in Ash.
    native_theme_cache_ = std::make_unique<NativeThemeCache>(
        *BrowserParamsProxy::Get()->NativeThemeInfo());

    // After construction finishes, start caching.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&LacrosService::StartNativeThemeCache,
                                  weak_factory_.GetWeakPtr()));
  }

  static crash_reporter::CrashKeyString<32> session_type("session-type");
  session_type.Set(
      SessionTypeToString(BrowserParamsProxy::Get()->SessionType()));

  // Short term workaround: if --crosapi-mojo-platform-channel-handle is
  // available, close --mojo-platform-channel-handle, and remove it
  // from command line. It is for backward compatibility support by
  // ash-chrome.
  // TODO(crbug.com/40170079): Remove this, when ash-chrome stops to support
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
  ConstructRemote<crosapi::mojom::AudioService, &Crosapi::BindAudioService,
                  Crosapi::MethodMinVersions::kBindAudioServiceMinVersion>();
  ConstructRemote<crosapi::mojom::CecPrivate, &Crosapi::BindCecPrivate,
                  Crosapi::MethodMinVersions::kBindCecPrivateMinVersion>();
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
  ConstructRemote<
      crosapi::mojom::CertProvisioning, &Crosapi::BindCertProvisioning,
      Crosapi::MethodMinVersions::kBindCertProvisioningMinVersion>();
  ConstructRemote<
      chromeos::cfm::mojom::CfmServiceContext,
      &crosapi::mojom::Crosapi::BindCfmServiceContext,
      Crosapi::MethodMinVersions::kBindCfmServiceContextMinVersion>();
  ConstructRemote<crosapi::mojom::ChapsService, &Crosapi::BindChapsService,
                  Crosapi::MethodMinVersions::kBindChapsServiceMinVersion>();
  ConstructRemote<crosapi::mojom::Clipboard, &Crosapi::BindClipboard,
                  Crosapi::MethodMinVersions::kBindClipboardMinVersion>();
  ConstructRemote<
      crosapi::mojom::ClipboardHistory, &Crosapi::BindClipboardHistory,
      Crosapi::MethodMinVersions::kBindClipboardHistoryMinVersion>();
  ConstructRemote<
      crosapi::mojom::ContentProtection, &Crosapi::BindContentProtection,
      Crosapi::MethodMinVersions::kBindContentProtectionMinVersion>();
  ConstructRemote<
      crosapi::mojom::CrosDisplayConfigController,
      &Crosapi::BindCrosDisplayConfigController,
      Crosapi::MethodMinVersions::kBindCrosDisplayConfigControllerMinVersion>();
  ConstructRemote<
      crosapi::mojom::DebugInterfaceRegisterer,
      &Crosapi::BindDebugInterfaceRegisterer,
      Crosapi::MethodMinVersions::kBindDebugInterfaceRegistererMinVersion>();
  ConstructRemote<crosapi::mojom::Desk, &Crosapi::BindDesk,
                  Crosapi::MethodMinVersions::kBindDeskMinVersion>();
  ConstructRemote<
      crosapi::mojom::DeskProfileObserver, &Crosapi::BindDeskProfileObserver,
      Crosapi::MethodMinVersions::kBindDeskProfileObserverMinVersion>();
  ConstructRemote<crosapi::mojom::DeskTemplate, &Crosapi::BindDeskTemplate,
                  Crosapi::MethodMinVersions::kBindDeskTemplateMinVersion>();
  ConstructRemote<
      crosapi::mojom::DeviceAttributes, &Crosapi::BindDeviceAttributes,
      Crosapi::MethodMinVersions::kBindDeviceAttributesMinVersion>();
  ConstructRemote<
      crosapi::mojom::DeviceOAuth2TokenService,
      &Crosapi::BindDeviceOAuth2TokenService,
      Crosapi::MethodMinVersions::kBindDeviceOAuth2TokenServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::DeviceSettingsService,
      &Crosapi::BindDeviceSettingsService,
      Crosapi::MethodMinVersions::kBindDeviceSettingsServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::DiagnosticsService, &Crosapi::BindDiagnosticsService,
      Crosapi::MethodMinVersions::kBindDiagnosticsServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::DigitalGoodsFactory, &Crosapi::BindDigitalGoodsFactory,
      Crosapi::MethodMinVersions::kBindDigitalGoodsFactoryMinVersion>();
  ConstructRemote<crosapi::mojom::Dlp, &Crosapi::BindDlp,
                  Crosapi::MethodMinVersions::kBindDlpMinVersion>();
  ConstructRemote<crosapi::mojom::DocumentScan,
                  &crosapi::mojom::Crosapi::BindDocumentScan,
                  Crosapi::MethodMinVersions::kBindDocumentScanMinVersion>();
  ConstructRemote<
      crosapi::mojom::DownloadController, &Crosapi::BindDownloadController,
      Crosapi::MethodMinVersions::kBindDownloadControllerMinVersion>();
  ConstructRemote<
      crosapi::mojom::DownloadStatusUpdater,
      &Crosapi::BindDownloadStatusUpdater,
      Crosapi::MethodMinVersions::kBindDownloadStatusUpdaterMinVersion>();
  ConstructRemote<
      crosapi::mojom::DriveIntegrationService,
      &crosapi::mojom::Crosapi::BindDriveIntegrationService,
      Crosapi::MethodMinVersions::kBindDriveIntegrationServiceMinVersion>();
  ConstructRemote<crosapi::mojom::EchoPrivate,
                  &crosapi::mojom::Crosapi::BindEchoPrivate,
                  Crosapi::MethodMinVersions::kBindEchoPrivateMinVersion>();
  if (chromeos::features::IsOrcaEnabled()) {
    ConstructRemote<
        crosapi::mojom::EditorPanelManager, &Crosapi::BindEditorPanelManager,
        Crosapi::MethodMinVersions::kBindEditorPanelManagerMinVersion>();
  }
  ConstructRemote<
      crosapi::mojom::EmbeddedAccessibilityHelperClientFactory,
      &Crosapi::BindEmbeddedAccessibilityHelperClientFactory,
      Crosapi::MethodMinVersions::
          kBindEmbeddedAccessibilityHelperClientFactoryMinVersion>();
  ConstructRemote<crosapi::mojom::EmojiPicker, &Crosapi::BindEmojiPicker,
                  Crosapi::MethodMinVersions::kBindEmojiPickerMinVersion>();
  ConstructRemote<
      crosapi::mojom::ExtensionInfoPrivate,
      &crosapi::mojom::Crosapi::BindExtensionInfoPrivate,
      Crosapi::MethodMinVersions::kBindExtensionInfoPrivateMinVersion>();
  ConstructRemote<
      crosapi::mojom::ExtensionPrinterService,
      &crosapi::mojom::Crosapi::BindExtensionPrinterService,
      Crosapi::MethodMinVersions::kBindExtensionPrinterServiceMinVersion>();
  ConstructRemote<crosapi::mojom::EyeDropper,
                  &crosapi::mojom::Crosapi::BindEyeDropper,
                  Crosapi::MethodMinVersions::kBindEyeDropperMinVersion>();
  ConstructRemote<crosapi::mojom::Feedback,
                  &crosapi::mojom::Crosapi::BindFeedback,
                  Crosapi::MethodMinVersions::kBindFeedbackMinVersion>();
  ConstructRemote<
      crosapi::mojom::FileChangeServiceBridge,
      &crosapi::mojom::Crosapi::BindFileChangeServiceBridge,
      Crosapi::MethodMinVersions::kBindFileChangeServiceBridgeMinVersion>();
  ConstructRemote<crosapi::mojom::FileManager,
                  &crosapi::mojom::Crosapi::BindFileManager,
                  Crosapi::MethodMinVersions::kBindFileManagerMinVersion>();
  ConstructRemote<
      crosapi::mojom::FileSystemProviderService,
      &crosapi::mojom::Crosapi::BindFileSystemProviderService,
      Crosapi::MethodMinVersions::kBindFileSystemProviderServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::FieldTrialService,
      &crosapi::mojom::Crosapi::BindFieldTrialService,
      Crosapi::MethodMinVersions::kBindFieldTrialServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::ForceInstalledTracker,
      &crosapi::mojom::Crosapi::BindForceInstalledTracker,
      Crosapi::MethodMinVersions::kBindForceInstalledTrackerMinVersion>();
  ConstructRemote<crosapi::mojom::FullRestore, &Crosapi::BindFullRestore,
                  Crosapi::MethodMinVersions::kBindFullRestoreMinVersion>();
  ConstructRemote<
      crosapi::mojom::FullscreenController, &Crosapi::BindFullscreenController,
      Crosapi::MethodMinVersions::kBindFullscreenControllerMinVersion>();
  ConstructRemote<
      crosapi::mojom::GeolocationService,
      &crosapi::mojom::Crosapi::BindGeolocationService,
      Crosapi::MethodMinVersions::kBindGeolocationServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::GuestOsSkForwarderFactory,
      &Crosapi::BindGuestOsSkForwarderFactory,
      Crosapi::MethodMinVersions::kBindGuestOsSkForwarderFactoryMinVersion>();
  ConstructRemote<device::mojom::HidManager,
                  &crosapi::mojom::Crosapi::BindHidManager,
                  Crosapi::MethodMinVersions::kBindHidManagerMinVersion>();
  ConstructRemote<crosapi::mojom::IdentityManager,
                  &crosapi::mojom::Crosapi::BindIdentityManager,
                  Crosapi::MethodMinVersions::kBindIdentityManagerMinVersion>();
  ConstructRemote<crosapi::mojom::IdleService,
                  &crosapi::mojom::Crosapi::BindIdleService,
                  Crosapi::MethodMinVersions::kBindIdleServiceMinVersion>();
  ConstructRemote<crosapi::mojom::ImageWriter,
                  &crosapi::mojom::Crosapi::BindImageWriter,
                  Crosapi::MethodMinVersions::kBindImageWriterMinVersion>();
  ConstructRemote<chromeos::auth::mojom::InSessionAuth,
                  &crosapi::mojom::Crosapi::BindInSessionAuth,
                  Crosapi::MethodMinVersions::kBindInSessionAuthMinVersion>();
  ConstructRemote<
      crosapi::mojom::KerberosInBrowser,
      &crosapi::mojom::Crosapi::BindKerberosInBrowser,
      Crosapi::MethodMinVersions::kBindKerberosInBrowserMinVersion>();
  ConstructRemote<crosapi::mojom::KeystoreService,
                  &crosapi::mojom::Crosapi::BindKeystoreService,
                  Crosapi::MethodMinVersions::kBindKeystoreServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::ChromeAppKioskService,
      &Crosapi::BindChromeAppKioskService,
      Crosapi::MethodMinVersions::kBindChromeAppKioskServiceMinVersion>();
  ConstructRemote<crosapi::mojom::WebKioskService,
                  &Crosapi::BindWebKioskService,
                  Crosapi::MethodMinVersions::kBindWebKioskServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::KioskSessionService, &Crosapi::BindKioskSessionService,
      Crosapi::MethodMinVersions::kBindKioskSessionServiceMinVersion>();
  ConstructRemote<crosapi::mojom::DeviceLocalAccountExtensionService,
                  &Crosapi::BindDeviceLocalAccountExtensionService,
                  Crosapi::MethodMinVersions::
                      kBindDeviceLocalAccountExtensionServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::LacrosShelfItemTracker,
      &crosapi::mojom::Crosapi::BindLacrosShelfItemTracker,
      Crosapi::MethodMinVersions::kBindLacrosShelfItemTrackerMinVersion>();
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
  ConstructRemote<
      crosapi::mojom::MagicBoostController,
      &crosapi::mojom::Crosapi::BindMagicBoostController,
      Crosapi::MethodMinVersions::kBindMagicBoostControllerMinVersion>();
  ConstructRemote<
      crosapi::mojom::MahiBrowserDelegate,
      &crosapi::mojom::Crosapi::BindMahiBrowserDelegate,
      Crosapi::MethodMinVersions::kBindMahiBrowserDelegateMinVersion>();
  ConstructRemote<crosapi::mojom::MediaUI,
                  &crosapi::mojom::Crosapi::BindMediaUI,
                  Crosapi::MethodMinVersions::kBindMediaUIMinVersion>();
  ConstructRemote<crosapi::mojom::MessageCenter,
                  &crosapi::mojom::Crosapi::BindMessageCenter,
                  Crosapi::MethodMinVersions::kBindMessageCenterMinVersion>();
  ConstructRemote<crosapi::mojom::Metrics,
                  &crosapi::mojom::Crosapi::BindMetrics,
                  Crosapi::MethodMinVersions::kBindMetricsMinVersion>();
  ConstructRemote<
      crosapi::mojom::MultiCaptureService,
      &crosapi::mojom::Crosapi::BindMultiCaptureService,
      Crosapi::MethodMinVersions::kBindMultiCaptureServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::NativeThemeService,
      &crosapi::mojom::Crosapi::BindNativeThemeService,
      Crosapi::MethodMinVersions::kBindNativeThemeServiceMinVersion>();
  ConstructRemote<crosapi::mojom::NetworkChange,
                  &crosapi::mojom::Crosapi::BindNetworkChange,
                  Crosapi::MethodMinVersions::kBindNetworkChangeMinVersion>();
  ConstructRemote<crosapi::mojom::Power, &crosapi::mojom::Crosapi::BindPower,
                  Crosapi::MethodMinVersions::kBindPowerMinVersion>();
  ConstructRemote<
      crosapi::mojom::NetworkingAttributes, &Crosapi::BindNetworkingAttributes,
      Crosapi::MethodMinVersions::kBindNetworkingAttributesMinVersion>();

  ConstructRemote<
      crosapi::mojom::NetworkingPrivate, &Crosapi::BindNetworkingPrivate,
      Crosapi::MethodMinVersions::kBindNetworkingPrivateMinVersion>();

  ConstructRemote<
      crosapi::mojom::OneDriveNotificationService,
      &crosapi::mojom::Crosapi::BindOneDriveNotificationService,
      Crosapi::MethodMinVersions::kBindOneDriveNotificationServiceMinVersion>();

  ConstructRemote<
      crosapi::mojom::OneDriveIntegrationService,
      &crosapi::mojom::Crosapi::BindOneDriveIntegrationService,
      Crosapi::MethodMinVersions::kBindOneDriveIntegrationServiceMinVersion>();

  ConstructRemote<crosapi::mojom::Prefs, &crosapi::mojom::Crosapi::BindPrefs,
                  Crosapi::MethodMinVersions::kBindPrefsMinVersion>();
  ConstructRemote<
      crosapi::mojom::NonclosableAppToastService,
      &crosapi::mojom::Crosapi::BindNonclosableAppToastService,
      Crosapi::MethodMinVersions::kBindNonclosableAppToastServiceMinVersion>();
  if (BrowserParamsProxy::Get()->UseCupsForPrinting()) {
    ConstructRemote<
        crosapi::mojom::PrintingMetrics,
        &crosapi::mojom::Crosapi::BindPrintingMetrics,
        Crosapi::MethodMinVersions::kBindPrintingMetricsMinVersion>();
  }
  ConstructRemote<
      crosapi::mojom::NetworkSettingsService,
      &crosapi::mojom::Crosapi::BindNetworkSettingsService,
      Crosapi::MethodMinVersions::kBindNetworkSettingsServiceMinVersion>();
  ConstructRemote<crosapi::mojom::ParentAccess, &Crosapi::BindParentAccess,
                  Crosapi::MethodMinVersions::kBindParentAccessMinVersion>();
  ConstructRemote<
      chromeos::payments::mojom::PaymentAppInstance,
      &Crosapi::BindPaymentAppInstance,
      Crosapi::MethodMinVersions::kBindPaymentAppInstanceMinVersion>();
  ConstructRemote<crosapi::mojom::PolicyService, &Crosapi::BindPolicyService,
                  Crosapi::MethodMinVersions::kBindPolicyServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::PrintPreviewCrosDelegate,
      &crosapi::mojom::Crosapi::BindPrintPreviewCrosDelegate,
      Crosapi::MethodMinVersions::kBindPrintPreviewCrosDelegateMinVersion>();
  ConstructRemote<
      chromeos::remote_apps::mojom::RemoteAppsLacrosBridge,
      &crosapi::mojom::Crosapi::BindRemoteAppsLacrosBridge,
      Crosapi::MethodMinVersions::kBindRemoteAppsLacrosBridgeMinVersion>();
  ConstructRemote<crosapi::mojom::Remoting,
                  &crosapi::mojom::Crosapi::BindRemoting,
                  Crosapi::MethodMinVersions::kBindRemotingMinVersion>();
  ConstructRemote<crosapi::mojom::ResourceManager,
                  &crosapi::mojom::Crosapi::BindResourceManager,
                  Crosapi::MethodMinVersions::kBindResourceManagerMinVersion>();
  ConstructRemote<
      crosapi::mojom::ScreenAIDownloader, &Crosapi::BindScreenAIDownloader,
      Crosapi::MethodMinVersions::kBindScreenAIDownloaderMinVersion>();
  ConstructRemote<crosapi::mojom::ScreenManager,
                  &crosapi::mojom::Crosapi::BindScreenManager,
                  Crosapi::MethodMinVersions::kBindScreenManagerMinVersion>();
  ConstructRemote<
      crosapi::mojom::SearchControllerRegistry,
      &crosapi::mojom::Crosapi::BindSearchControllerRegistry,
      Crosapi::MethodMinVersions::kBindSearchControllerRegistryMinVersion>();
  ConstructRemote<crosapi::mojom::Sharesheet,
                  &crosapi::mojom::Crosapi::BindSharesheet,
                  Crosapi::MethodMinVersions::kBindSharesheetMinVersion>();
  ConstructRemote<
      crosapi::mojom::SpeechRecognition,
      &crosapi::mojom::Crosapi::BindSpeechRecognition,
      Crosapi::MethodMinVersions::kBindSpeechRecognitionMinVersion>();
  ConstructRemote<
      crosapi::mojom::StructuredMetricsService,
      &crosapi::mojom::Crosapi::BindStructuredMetricsService,
      Crosapi::MethodMinVersions::kBindStructuredMetricsServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::SuggestionService,
      &crosapi::mojom::Crosapi::BindSuggestionService,
      Crosapi::MethodMinVersions::kBindSuggestionServiceMinVersion>();
  ConstructRemote<crosapi::mojom::SyncService,
                  &crosapi::mojom::Crosapi::BindSyncService,
                  Crosapi::MethodMinVersions::kBindSyncServiceMinVersion>();
  ConstructRemote<crosapi::mojom::TaskManager,
                  &crosapi::mojom::Crosapi::BindTaskManager,
                  Crosapi::MethodMinVersions::kBindTaskManagerMinVersion>();
  ConstructRemote<crosapi::mojom::TelemetryDiagnosticRoutinesService,
                  &Crosapi::BindTelemetryDiagnosticRoutinesService,
                  Crosapi::MethodMinVersions::
                      kBindTelemetryDiagnosticRoutinesServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::TelemetryEventService,
      &Crosapi::BindTelemetryEventService,
      Crosapi::MethodMinVersions::kBindTelemetryEventServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::TelemetryManagementService,
      &Crosapi::BindTelemetryManagementService,
      Crosapi::MethodMinVersions::kBindTelemetryManagementServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::TelemetryProbeService,
      &crosapi::mojom::Crosapi::BindTelemetryProbeService,
      Crosapi::MethodMinVersions::kBindTelemetryProbeServiceMinVersion>();
  ConstructRemote<crosapi::mojom::TimeZoneService,
                  &crosapi::mojom::Crosapi::BindTimeZoneService,
                  Crosapi::MethodMinVersions::kBindTimeZoneServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::TrustedVaultBackend,
      &crosapi::mojom::Crosapi::BindTrustedVaultBackend,
      Crosapi::MethodMinVersions::kBindTrustedVaultBackendMinVersion>();
  ConstructRemote<
      crosapi::mojom::TrustedVaultBackendService,
      &crosapi::mojom::Crosapi::BindTrustedVaultBackendService,
      Crosapi::MethodMinVersions::kBindTrustedVaultBackendServiceMinVersion>();
  ConstructRemote<crosapi::mojom::Tts, &crosapi::mojom::Crosapi::BindTts,
                  Crosapi::MethodMinVersions::kBindTtsMinVersion>();
  ConstructRemote<crosapi::mojom::UrlHandler,
                  &crosapi::mojom::Crosapi::BindUrlHandler,
                  Crosapi::MethodMinVersions::kBindUrlHandlerMinVersion>();
  ConstructRemote<
      crosapi::mojom::VideoConferenceManager,
      &crosapi::mojom::Crosapi::BindVideoConferenceManager,
      Crosapi::MethodMinVersions::kBindVideoConferenceManagerMinVersion>();
  ConstructRemote<crosapi::mojom::AppPublisher, &Crosapi::BindWebAppPublisher,
                  Crosapi::MethodMinVersions::kBindWebAppPublisherMinVersion>();
  ConstructRemote<crosapi::mojom::Wallpaper,
                  &crosapi::mojom::Crosapi::BindWallpaper,
                  Crosapi::MethodMinVersions::kBindWallpaperMinVersion>();
  ConstructRemote<crosapi::mojom::WebAppService,
                  &crosapi::mojom::Crosapi::BindWebAppService,
                  Crosapi::MethodMinVersions::kBindWebAppServiceMinVersion>();
  ConstructRemote<
      crosapi::mojom::WebPageInfoFactory,
      &crosapi::mojom::Crosapi::BindWebPageInfoFactory,
      Crosapi::MethodMinVersions::kBindWebPageInfoFactoryMinVersion>();
  ConstructRemote<crosapi::mojom::VolumeManager,
                  &crosapi::mojom::Crosapi::BindVolumeManager,
                  Crosapi::MethodMinVersions::kBindVolumeManagerMinVersion>();
  ConstructRemote<
      crosapi::mojom::VpnExtensionObserver, &Crosapi::BindVpnExtensionObserver,
      Crosapi::MethodMinVersions::kBindVpnExtensionObserverMinVersion>();
  ConstructRemote<crosapi::mojom::VpnService, &Crosapi::BindVpnService,
                  Crosapi::MethodMinVersions::kBindVpnServiceMinVersion>();
  ConstructRemote<crosapi::mojom::VirtualKeyboard,
                  &crosapi::mojom::Crosapi::BindVirtualKeyboard,
                  Crosapi::MethodMinVersions::kBindVirtualKeyboardMinVersion>();
  ConstructRemote<
      crosapi::mojom::FileSystemAccessCloudIdentifierProvider,
      &crosapi::mojom::Crosapi::BindFileSystemAccessCloudIdentifierProvider,
      Crosapi::MethodMinVersions::
          kBindFileSystemAccessCloudIdentifierProviderMinVersion>();
  ConstructRemote<crosapi::mojom::InputMethods, &Crosapi::BindInputMethods,
                  Crosapi::MethodMinVersions::kBindInputMethodsMinVersion>();

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
  if (!command_line->HasSwitch(crosapi::kCrosapiMojoPlatformChannelHandle)) {
    return;
  }

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
      entry.second->MaybeBind(this);
    }
  }

  if (IsOnBrowserStartupAvailable()) {
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&LacrosServiceNeverBlockingState::OnBrowserStartup,
                       weak_sequenced_state_, ToMojo(browser_version)));
  }
}

void LacrosService::BindAccountManagerReceiver(
    mojo::PendingReceiver<crosapi::mojom::AccountManager> pending_receiver) {
  DCHECK(IsSupported<crosapi::mojom::AccountManager>());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::AccountManager>,
      &crosapi::mojom::Crosapi::BindAccountManager>(
      std::move(pending_receiver));
}

void LacrosService::BindAudioFocusManager(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManager> remote) {
  DCHECK(IsSupported<media_session::mojom::AudioFocusManager>());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager>,
      &crosapi::mojom::Crosapi::BindMediaSessionAudioFocus>(std::move(remote));
}

void LacrosService::BindAudioFocusManagerDebug(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
        remote) {
  DCHECK(IsSupported<media_session::mojom::AudioFocusManager>());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>,
      &crosapi::mojom::Crosapi::BindMediaSessionAudioFocusDebug>(
      std::move(remote));
}

void LacrosService::BindBrowserCdmFactory(
    mojo::GenericPendingReceiver receiver) {
  DCHECK(IsSupported<cdm::mojom::BrowserCdmFactory>());
  BindPendingReceiverOrRemote<mojo::GenericPendingReceiver,
                              &crosapi::mojom::Crosapi::BindBrowserCdmFactory>(
      std::move(receiver));
}

void LacrosService::BindCfmServiceContext(
    mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext> receiver) {
  DCHECK(IsSupported<chromeos::cfm::mojom::CfmServiceContext>());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext>,
      &crosapi::mojom::Crosapi::BindCfmServiceContext>(std::move(receiver));
}

void LacrosService::BindGeolocationService(
    mojo::PendingReceiver<crosapi::mojom::GeolocationService>
        pending_receiver) {
  DCHECK(IsSupported<crosapi::mojom::GeolocationService>());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::GeolocationService>,
      &crosapi::mojom::Crosapi::BindGeolocationService>(
      std::move(pending_receiver));
}

void LacrosService::BindMachineLearningService(
    mojo::PendingReceiver<
        chromeos::machine_learning::mojom::MachineLearningService> receiver) {
  DCHECK(
      IsSupported<chromeos::machine_learning::mojom::MachineLearningService>());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<
          chromeos::machine_learning::mojom::MachineLearningService>,
      &crosapi::mojom::Crosapi::BindMachineLearningService>(
      std::move(receiver));
}

void LacrosService::BindMagicBoostController(
    mojo::PendingReceiver<crosapi::mojom::MagicBoostController>
        pending_receiver) {
  DCHECK(IsSupported<crosapi::mojom::MagicBoostController>());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::MagicBoostController>,
      &crosapi::mojom::Crosapi::BindMagicBoostController>(
      std::move(pending_receiver));
}

void LacrosService::BindMahiBrowserDelegate(
    mojo::PendingReceiver<crosapi::mojom::MahiBrowserDelegate>
        pending_receiver) {
  DCHECK(IsSupported<crosapi::mojom::MahiBrowserDelegate>());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::MahiBrowserDelegate>,
      &crosapi::mojom::Crosapi::BindMahiBrowserDelegate>(
      std::move(pending_receiver));
}

void LacrosService::BindMediaControllerManager(
    mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
        remote) {
  DCHECK(IsSupported<media_session::mojom::AudioFocusManager>());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>,
      &crosapi::mojom::Crosapi::BindMediaSessionController>(std::move(remote));
}

void LacrosService::BindMetricsReporting(
    mojo::PendingReceiver<crosapi::mojom::MetricsReporting> receiver) {
  DCHECK(IsSupported<crosapi::mojom::MetricsReporting>());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::MetricsReporting>,
      &crosapi::mojom::Crosapi::BindMetricsReporting>(std::move(receiver));
}

void LacrosService::BindPrintPreviewCrosDelegate(
    mojo::PendingReceiver<crosapi::mojom::PrintPreviewCrosDelegate>
        pending_receiver) {
  DCHECK(IsSupported<crosapi::mojom::PrintPreviewCrosDelegate>());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::PrintPreviewCrosDelegate>,
      &crosapi::mojom::Crosapi::BindPrintPreviewCrosDelegate>(
      std::move(pending_receiver));
}

void LacrosService::BindRemoteAppsLacrosBridge(
    mojo::PendingReceiver<chromeos::remote_apps::mojom::RemoteAppsLacrosBridge>
        receiver) {
  DCHECK(IsSupported<chromeos::remote_apps::mojom::RemoteAppsLacrosBridge>());

  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<
          chromeos::remote_apps::mojom::RemoteAppsLacrosBridge>,
      &crosapi::mojom::Crosapi::BindRemoteAppsLacrosBridge>(
      std::move(receiver));
}

void LacrosService::BindScreenManagerReceiver(
    mojo::PendingReceiver<crosapi::mojom::ScreenManager> pending_receiver) {
  DCHECK(IsSupported<crosapi::mojom::ScreenManager>());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::ScreenManager>,
      &crosapi::mojom::Crosapi::BindScreenManager>(std::move(pending_receiver));
}

void LacrosService::BindSensorHalClient(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote) {
  DCHECK(IsSupported<chromeos::sensors::mojom::SensorHalClient>());
  BindPendingReceiverOrRemote<
      mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient>,
      &crosapi::mojom::Crosapi::BindSensorHalClient>(std::move(remote));
}

void LacrosService::BindMediaApp(
    mojo::PendingRemote<crosapi::mojom::MediaApp> remote) {
  DCHECK(IsSupported<crosapi::mojom::MediaApp>());
  BindPendingReceiverOrRemote<mojo::PendingRemote<crosapi::mojom::MediaApp>,
                              &crosapi::mojom::Crosapi::BindMediaApp>(
      std::move(remote));
}

bool LacrosService::IsOnBrowserStartupAvailable() const {
  std::optional<uint32_t> version = CrosapiVersion();
  return version && version.value() >=
                        Crosapi::MethodMinVersions::kOnBrowserStartupMinVersion;
}

void LacrosService::BindVideoCaptureDeviceFactory(
    mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory>
        pending_receiver) {
  DCHECK(IsSupported<crosapi::mojom::VideoCaptureDeviceFactory>());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory>,
      &crosapi::mojom::Crosapi::BindVideoCaptureDeviceFactory>(
      std::move(pending_receiver));
}

void LacrosService::BindVideoConferenceManager(
    mojo::PendingReceiver<crosapi::mojom::VideoConferenceManager>
        pending_receiver) {
  DCHECK(IsSupported<crosapi::mojom::VideoConferenceManager>());
  BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::VideoConferenceManager>,
      &crosapi::mojom::Crosapi::BindVideoConferenceManager>(
      std::move(pending_receiver));
}

void LacrosService::BindStableVideoDecoderFactory(
    mojo::PendingReceiver<media::stable::mojom::StableVideoDecoderFactory>
        receiver) {
  DCHECK(IsSupported<media::stable::mojom::StableVideoDecoderFactory>());
  BindPendingReceiverOrRemote<
      mojo::GenericPendingReceiver,
      &crosapi::mojom::Crosapi::BindStableVideoDecoderFactory>(
      mojo::GenericPendingReceiver(std::move(receiver)));
}

std::optional<uint32_t> LacrosService::CrosapiVersion() const {
  DCHECK(did_bind_receiver_);
  return chromeos::BrowserParamsProxy::Get()->CrosapiVersion();
}

int LacrosService::GetInterfaceVersion(base::Token interface_uuid) const {
  if (!chromeos::BrowserParamsProxy::Get()->InterfaceVersions()) {
    return -1;
  }
  const base::flat_map<base::Token, uint32_t>& versions =
      chromeos::BrowserParamsProxy::Get()->InterfaceVersions().value();
  auto it = versions.find(interface_uuid);
  if (it == versions.end()) {
    return -1;
  }
  return it->second;
}

void LacrosService::StartSystemIdleCache() {
  system_idle_cache_->Start();
}

void LacrosService::StartNativeThemeCache() {
  native_theme_cache_->Start();
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

void LacrosService::NotifyPolicyFetchAttempt() {
  observer_list_->Notify(FROM_HERE, &Observer::OnPolicyFetchAttempt);
}

void LacrosService::NotifyComponentPolicyUpdated(ComponentPolicyMap policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  observer_list_->Notify(FROM_HERE, &Observer::OnComponentPolicyUpdated,
                         std::move(policy));
}

}  // namespace chromeos
