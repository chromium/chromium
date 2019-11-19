// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_arc_bridge_host.h"

#include "components/arc/mojom/accessibility_helper.mojom.h"
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
#include "components/arc/mojom/print.mojom.h"
#include "components/arc/mojom/print_spooler.mojom.h"
#include "components/arc/mojom/process.mojom.h"
#include "components/arc/mojom/property.mojom.h"
#include "components/arc/mojom/rotation_lock.mojom.h"
#include "components/arc/mojom/screen_capture.mojom.h"
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
    mojom::AccessibilityHelperInstancePtr accessibility_helper_ptr) {}

void FakeArcBridgeHost::OnAppInstanceReady(mojom::AppInstancePtr app_ptr) {}

void FakeArcBridgeHost::OnAppPermissionsInstanceReady(
    mojom::AppPermissionsInstancePtr app_permissions_ptr) {}

void FakeArcBridgeHost::OnAppfuseInstanceReady(
    mojom::AppfuseInstancePtr app_ptr) {}

void FakeArcBridgeHost::OnAudioInstanceReady(
    mojom::AudioInstancePtr audio_ptr) {}

void FakeArcBridgeHost::OnAuthInstanceReady(mojom::AuthInstancePtr auth_ptr) {}

void FakeArcBridgeHost::OnBackupSettingsInstanceReady(
    mojom::BackupSettingsInstancePtr backup_settings_ptr) {}

void FakeArcBridgeHost::OnBluetoothInstanceReady(
    mojom::BluetoothInstancePtr bluetooth_ptr) {}

void FakeArcBridgeHost::OnBootPhaseMonitorInstanceReady(
    mojom::BootPhaseMonitorInstancePtr boot_phase_monitor_ptr) {}

void FakeArcBridgeHost::OnCameraInstanceReady(
    mojom::CameraInstancePtr camera_ptr) {}

void FakeArcBridgeHost::OnCastReceiverInstanceReady(
    mojom::CastReceiverInstancePtr cast_receiver_ptr) {}

void FakeArcBridgeHost::OnCertStoreInstanceReady(
    mojom::CertStoreInstancePtr instance_ptr) {}

void FakeArcBridgeHost::OnClipboardInstanceReady(
    mojom::ClipboardInstancePtr clipboard_ptr) {}

void FakeArcBridgeHost::OnCrashCollectorInstanceReady(
    mojom::CrashCollectorInstancePtr crash_collector_ptr) {}

void FakeArcBridgeHost::OnDiskQuotaInstanceReady(
    mojom::DiskQuotaInstancePtr disk_quota_ptr) {}

void FakeArcBridgeHost::OnEnterpriseReportingInstanceReady(
    mojom::EnterpriseReportingInstancePtr enterprise_reporting_ptr) {}

void FakeArcBridgeHost::OnFileSystemInstanceReady(
    mojom::FileSystemInstancePtr file_system_ptr) {}

void FakeArcBridgeHost::OnImeInstanceReady(mojom::ImeInstancePtr ime_ptr) {}

void FakeArcBridgeHost::OnInputMethodManagerInstanceReady(
    mojom::InputMethodManagerInstancePtr input_method_manager_ptr) {}

void FakeArcBridgeHost::OnIntentHelperInstanceReady(
    mojom::IntentHelperInstancePtr intent_helper_ptr) {}

void FakeArcBridgeHost::OnKeymasterInstanceReady(
    mojom::KeymasterInstancePtr keymaster_ptr) {}

void FakeArcBridgeHost::OnKioskInstanceReady(
    mojom::KioskInstancePtr kiosk_ptr) {}

void FakeArcBridgeHost::OnLockScreenInstanceReady(
    mojom::LockScreenInstancePtr lock_screen_ptr) {}

void FakeArcBridgeHost::OnMediaSessionInstanceReady(
    mojom::MediaSessionInstancePtr media_sesssion_ptr) {}

void FakeArcBridgeHost::OnMetricsInstanceReady(
    mojom::MetricsInstancePtr metrics_ptr) {}

void FakeArcBridgeHost::OnMidisInstanceReady(
    mojom::MidisInstancePtr midis_ptr) {}

void FakeArcBridgeHost::OnNetInstanceReady(mojom::NetInstancePtr net_ptr) {}

void FakeArcBridgeHost::OnNotificationsInstanceReady(
    mojom::NotificationsInstancePtr notifications_ptr) {}

void FakeArcBridgeHost::OnObbMounterInstanceReady(
    mojom::ObbMounterInstancePtr obb_mounter_ptr) {}

void FakeArcBridgeHost::OnOemCryptoInstanceReady(
    mojom::OemCryptoInstancePtr oemcrypto_ptr) {}

void FakeArcBridgeHost::OnPipInstanceReady(mojom::PipInstancePtr pip_ptr) {}

void FakeArcBridgeHost::OnPolicyInstanceReady(
    mojom::PolicyInstancePtr policy_ptr) {}

void FakeArcBridgeHost::OnPowerInstanceReady(
    mojom::PowerInstancePtr power_ptr) {}

void FakeArcBridgeHost::OnPrintInstanceReady(
    mojom::PrintInstancePtr print_ptr) {}

void FakeArcBridgeHost::OnPrintSpoolerInstanceReady(
    mojom::PrintSpoolerInstancePtr print_spooler_ptr) {}

void FakeArcBridgeHost::OnProcessInstanceReady(
    mojom::ProcessInstancePtr process_ptr) {}

void FakeArcBridgeHost::OnPropertyInstanceReady(
    mojom::PropertyInstancePtr property_ptr) {}

void FakeArcBridgeHost::OnRotationLockInstanceReady(
    mojom::RotationLockInstancePtr rotation_lock_ptr) {}

void FakeArcBridgeHost::OnScreenCaptureInstanceReady(
    mojom::ScreenCaptureInstancePtr screen_capture_ptr) {}

void FakeArcBridgeHost::OnSmartCardManagerInstanceReady(
    mojom::SmartCardManagerInstancePtr smart_cardManager_ptr) {}

void FakeArcBridgeHost::OnStorageManagerInstanceReady(
    mojom::StorageManagerInstancePtr storage_manager_ptr) {}

void FakeArcBridgeHost::OnTimerInstanceReady(
    mojom::TimerInstancePtr timer_ptr) {}

void FakeArcBridgeHost::OnTracingInstanceReady(
    mojom::TracingInstancePtr trace_ptr) {}

void FakeArcBridgeHost::OnTtsInstanceReady(mojom::TtsInstancePtr tts_ptr) {}

void FakeArcBridgeHost::OnUsbHostInstanceReady(
    mojom::UsbHostInstancePtr usb_ptr) {}

void FakeArcBridgeHost::OnVideoInstanceReady(
    mojom::VideoInstancePtr video_ptr) {}

void FakeArcBridgeHost::OnVoiceInteractionArcHomeInstanceReady(
    mojom::VoiceInteractionArcHomeInstancePtr home_ptr) {}

void FakeArcBridgeHost::OnVoiceInteractionFrameworkInstanceReady(
    mojom::VoiceInteractionFrameworkInstancePtr framework_ptr) {}

void FakeArcBridgeHost::OnVolumeMounterInstanceReady(
    mojom::VolumeMounterInstancePtr volume_mounter_ptr) {}

void FakeArcBridgeHost::OnWakeLockInstanceReady(
    mojom::WakeLockInstancePtr wakelock_ptr) {}

void FakeArcBridgeHost::OnWallpaperInstanceReady(
    mojom::WallpaperInstancePtr wallpaper_ptr) {}

}  // namespace arc
