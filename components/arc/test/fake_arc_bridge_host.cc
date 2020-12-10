// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_arc_bridge_host.h"

#include "components/arc/mojom/accessibility_helper.mojom.h"
#include "components/arc/mojom/adbd.mojom.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/mojom/app_permissions.mojom.h"
#include "components/arc/mojom/appfuse.mojom.h"
#include "components/arc/mojom/arc_bridge.mojom.h"
#include "components/arc/mojom/audio.mojom.h"
#include "components/arc/mojom/auth.mojom.h"
#include "components/arc/mojom/backup_settings.mojom.h"
#include "components/arc/mojom/bluetooth.mojom.h"
#include "components/arc/mojom/boot_phase_monitor.mojom.h"
#include "components/arc/mojom/camera.mojom.h"
#include "components/arc/mojom/cast_receiver.mojom.h"
#include "components/arc/mojom/cert_store.mojom.h"
#include "components/arc/mojom/clipboard.mojom.h"
#include "components/arc/mojom/crash_collector.mojom.h"
#include "components/arc/mojom/disk_quota.mojom.h"
#include "components/arc/mojom/enterprise_reporting.mojom.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "components/arc/mojom/ime.mojom.h"
#include "components/arc/mojom/input_method_manager.mojom.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/arc/mojom/keymaster.mojom.h"
#include "components/arc/mojom/kiosk.mojom.h"
#include "components/arc/mojom/lock_screen.mojom.h"
#include "components/arc/mojom/media_session.mojom.h"
#include "components/arc/mojom/metrics.mojom.h"
#include "components/arc/mojom/midis.mojom.h"
#include "components/arc/mojom/net.mojom.h"
#include "components/arc/mojom/notifications.mojom.h"
#include "components/arc/mojom/obb_mounter.mojom.h"
#include "components/arc/mojom/oemcrypto.mojom.h"
#include "components/arc/mojom/pip.mojom.h"
#include "components/arc/mojom/policy.mojom.h"
#include "components/arc/mojom/power.mojom.h"
#include "components/arc/mojom/print_spooler.mojom.h"
#include "components/arc/mojom/process.mojom.h"
#include "components/arc/mojom/property.mojom.h"
#include "components/arc/mojom/rotation_lock.mojom.h"
#include "components/arc/mojom/screen_capture.mojom.h"
#include "components/arc/mojom/sensor.mojom.h"
#include "components/arc/mojom/sharesheet.mojom.h"
#include "components/arc/mojom/storage_manager.mojom.h"
#include "components/arc/mojom/timer.mojom.h"
#include "components/arc/mojom/tracing.mojom.h"
#include "components/arc/mojom/tts.mojom.h"
#include "components/arc/mojom/usb_host.mojom.h"
#include "components/arc/mojom/video.mojom.h"
#include "components/arc/mojom/voice_interaction_arc_home.mojom.h"
#include "components/arc/mojom/voice_interaction_framework.mojom.h"
#include "components/arc/mojom/volume_mounter.mojom.h"
#include "components/arc/mojom/wake_lock.mojom.h"
#include "components/arc/mojom/wallpaper.mojom.h"

namespace arc {

FakeArcBridgeHost::FakeArcBridgeHost() = default;

FakeArcBridgeHost::~FakeArcBridgeHost() = default;

void FakeArcBridgeHost::OnAccessibilityHelperInstanceReady(
    mojo::PendingRemote<mojom::AccessibilityHelperInstance>
        accessibility_helper_remote) {}

void FakeArcBridgeHost::OnAdbdMonitorInstanceReady(
    mojo::PendingRemote<mojom::AdbdMonitorInstance> adbd_monitor_remote) {}

void FakeArcBridgeHost::OnAppInstanceReady(
    mojo::PendingRemote<mojom::AppInstance> app_ptr) {}

void FakeArcBridgeHost::OnAppPermissionsInstanceReady(
    mojo::PendingRemote<mojom::AppPermissionsInstance> app_permissions_remote) {
}

void FakeArcBridgeHost::OnAppfuseInstanceReady(
    mojo::PendingRemote<mojom::AppfuseInstance> app_remote) {}

void FakeArcBridgeHost::OnAudioInstanceReady(
    mojo::PendingRemote<mojom::AudioInstance> audio_remote) {}

void FakeArcBridgeHost::OnAuthInstanceReady(
    mojo::PendingRemote<mojom::AuthInstance> auth_remote) {}

void FakeArcBridgeHost::OnBackupSettingsInstanceReady(
    mojo::PendingRemote<mojom::BackupSettingsInstance> backup_settings_remote) {
}

void FakeArcBridgeHost::OnBluetoothInstanceReady(
    mojo::PendingRemote<mojom::BluetoothInstance> bluetooth_remote) {}

void FakeArcBridgeHost::OnBootPhaseMonitorInstanceReady(
    mojo::PendingRemote<mojom::BootPhaseMonitorInstance>
        boot_phase_monitor_remote) {}

void FakeArcBridgeHost::OnCameraInstanceReady(
    mojo::PendingRemote<mojom::CameraInstance> camera_remote) {}

void FakeArcBridgeHost::OnCastReceiverInstanceReady(
    mojo::PendingRemote<mojom::CastReceiverInstance> cast_receiver_remote) {}

void FakeArcBridgeHost::OnCertStoreInstanceReady(
    mojo::PendingRemote<mojom::CertStoreInstance> instance_remote) {}

void FakeArcBridgeHost::OnClipboardInstanceReady(
    mojo::PendingRemote<mojom::ClipboardInstance> clipboard_remote) {}

void FakeArcBridgeHost::OnCrashCollectorInstanceReady(
    mojo::PendingRemote<mojom::CrashCollectorInstance> crash_collector_remote) {
}

void FakeArcBridgeHost::OnDigitalGoodsInstanceReady(
    mojo::PendingRemote<mojom::DigitalGoodsInstance> digital_goods_remote) {}

void FakeArcBridgeHost::OnDiskQuotaInstanceReady(
    mojo::PendingRemote<mojom::DiskQuotaInstance> disk_quota_remote) {}

void FakeArcBridgeHost::OnEnterpriseReportingInstanceReady(
    mojo::PendingRemote<mojom::EnterpriseReportingInstance>
        enterprise_reporting_remote) {}

void FakeArcBridgeHost::OnFileSystemInstanceReady(
    mojo::PendingRemote<mojom::FileSystemInstance> file_system_remote) {}

void FakeArcBridgeHost::OnImeInstanceReady(
    mojo::PendingRemote<mojom::ImeInstance> ime_remote) {}

void FakeArcBridgeHost::OnInputMethodManagerInstanceReady(
    mojo::PendingRemote<mojom::InputMethodManagerInstance>
        input_method_manager_remote) {}

void FakeArcBridgeHost::OnIntentHelperInstanceReady(
    mojo::PendingRemote<mojom::IntentHelperInstance> intent_helper_remote) {}

void FakeArcBridgeHost::OnKeymasterInstanceReady(
    mojo::PendingRemote<mojom::KeymasterInstance> keymaster_remote) {}

void FakeArcBridgeHost::OnKioskInstanceReady(
    mojo::PendingRemote<mojom::KioskInstance> kiosk_remote) {}

void FakeArcBridgeHost::OnLockScreenInstanceReady(
    mojo::PendingRemote<mojom::LockScreenInstance> lock_screen_remote) {}

void FakeArcBridgeHost::OnMediaSessionInstanceReady(
    mojo::PendingRemote<mojom::MediaSessionInstance> media_sesssion_remote) {}

void FakeArcBridgeHost::OnMetricsInstanceReady(
    mojo::PendingRemote<mojom::MetricsInstance> metrics_remote) {}

void FakeArcBridgeHost::OnMidisInstanceReady(
    mojo::PendingRemote<mojom::MidisInstance> midis_remote) {}

void FakeArcBridgeHost::OnNetInstanceReady(
    mojo::PendingRemote<mojom::NetInstance> net_remote) {}

void FakeArcBridgeHost::OnNotificationsInstanceReady(
    mojo::PendingRemote<mojom::NotificationsInstance> notifications_remote) {}

void FakeArcBridgeHost::OnObbMounterInstanceReady(
    mojo::PendingRemote<mojom::ObbMounterInstance> obb_mounter_remote) {}

void FakeArcBridgeHost::OnOemCryptoInstanceReady(
    mojo::PendingRemote<mojom::OemCryptoInstance> oemcrypto_remote) {}

void FakeArcBridgeHost::OnPaymentAppInstanceReady(
    mojo::PendingRemote<mojom::PaymentAppInstance> payment_app_remote) {}

void FakeArcBridgeHost::OnPipInstanceReady(
    mojo::PendingRemote<mojom::PipInstance> pip_remote) {}

void FakeArcBridgeHost::OnPolicyInstanceReady(
    mojo::PendingRemote<mojom::PolicyInstance> policy_remote) {}

void FakeArcBridgeHost::OnPowerInstanceReady(
    mojo::PendingRemote<mojom::PowerInstance> power_remote) {}

void FakeArcBridgeHost::OnPrintSpoolerInstanceReady(
    mojo::PendingRemote<mojom::PrintSpoolerInstance> print_spooler_remote) {}

void FakeArcBridgeHost::OnProcessInstanceReady(
    mojo::PendingRemote<mojom::ProcessInstance> process_remote) {}

void FakeArcBridgeHost::OnPropertyInstanceReady(
    mojo::PendingRemote<mojom::PropertyInstance> property_remote) {}

void FakeArcBridgeHost::OnRotationLockInstanceReady(
    mojo::PendingRemote<mojom::RotationLockInstance> rotation_lock_remote) {}

void FakeArcBridgeHost::OnScreenCaptureInstanceReady(
    mojo::PendingRemote<mojom::ScreenCaptureInstance> screen_capture_remote) {}

void FakeArcBridgeHost::OnSensorInstanceReady(
    mojo::PendingRemote<mojom::SensorInstance> sensor_remote) {}

void FakeArcBridgeHost::OnSharesheetInstanceReady(
    mojo::PendingRemote<mojom::SharesheetInstance> sharesheet_remote) {}

void FakeArcBridgeHost::OnSmartCardManagerInstanceReady(
    mojo::PendingRemote<mojom::SmartCardManagerInstance>
        smart_cardManager_remote) {}

void FakeArcBridgeHost::OnStorageManagerInstanceReady(
    mojo::PendingRemote<mojom::StorageManagerInstance> storage_manager_remote) {
}

void FakeArcBridgeHost::OnTimerInstanceReady(
    mojo::PendingRemote<mojom::TimerInstance> timer_remote) {}

void FakeArcBridgeHost::OnTracingInstanceReady(
    mojo::PendingRemote<mojom::TracingInstance> trace_remote) {}

void FakeArcBridgeHost::OnTtsInstanceReady(
    mojo::PendingRemote<mojom::TtsInstance> tts_remote) {}

void FakeArcBridgeHost::OnUsbHostInstanceReady(
    mojo::PendingRemote<mojom::UsbHostInstance> usb_remote) {}

void FakeArcBridgeHost::OnVideoInstanceReady(
    mojo::PendingRemote<mojom::VideoInstance> video_remote) {}

void FakeArcBridgeHost::OnVoiceInteractionArcHomeInstanceReady(
    mojo::PendingRemote<mojom::VoiceInteractionArcHomeInstance> home_remote) {}

void FakeArcBridgeHost::OnVoiceInteractionFrameworkInstanceReady(
    mojo::PendingRemote<mojom::VoiceInteractionFrameworkInstance>
        framework_remote) {}

void FakeArcBridgeHost::OnVolumeMounterInstanceReady(
    mojo::PendingRemote<mojom::VolumeMounterInstance> volume_mounter_remote) {}

void FakeArcBridgeHost::OnWakeLockInstanceReady(
    mojo::PendingRemote<mojom::WakeLockInstance> wakelock_remote) {}

void FakeArcBridgeHost::OnWallpaperInstanceReady(
    mojo::PendingRemote<mojom::WallpaperInstance> wallpaper_remote) {}

}  // namespace arc
