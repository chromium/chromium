// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_HOST_H_
#define COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_HOST_H_

#include "base/macros.h"
#include "components/arc/mojom/arc_bridge.mojom.h"

namespace arc {

class FakeArcBridgeHost : public mojom::ArcBridgeHost {
 public:
  FakeArcBridgeHost();
  ~FakeArcBridgeHost() override;

  // ArcBridgeHost overrides.
  void OnAccessibilityHelperInstanceReady(
      mojom::AccessibilityHelperInstancePtr accessibility_helper_ptr) override;
  void OnAppInstanceReady(mojom::AppInstancePtr app_ptr) override;
  void OnAppPermissionsInstanceReady(
      mojom::AppPermissionsInstancePtr app_permissions_ptr) override;
  void OnAppfuseInstanceReady(mojom::AppfuseInstancePtr appfuse_ptr) override;
  void OnAudioInstanceReady(mojom::AudioInstancePtr audio_ptr) override;
  void OnAuthInstanceReady(mojom::AuthInstancePtr auth_ptr) override;
  void OnBackupSettingsInstanceReady(
      mojom::BackupSettingsInstancePtr backup_settings_ptr) override;
  void OnBluetoothInstanceReady(
      mojom::BluetoothInstancePtr bluetooth_ptr) override;
  void OnBootPhaseMonitorInstanceReady(
      mojom::BootPhaseMonitorInstancePtr boot_phase_monitor_ptr) override;
  void OnCameraInstanceReady(mojom::CameraInstancePtr camera_ptr) override;
  void OnCastReceiverInstanceReady(
      mojom::CastReceiverInstancePtr cast_receiver_ptr) override;
  void OnCertStoreInstanceReady(
      mojom::CertStoreInstancePtr instance_ptr) override;
  void OnClipboardInstanceReady(
      mojom::ClipboardInstancePtr clipboard_ptr) override;
  void OnCrashCollectorInstanceReady(
      mojom::CrashCollectorInstancePtr crash_collector_ptr) override;
  void OnDiskQuotaInstanceReady(
      mojom::DiskQuotaInstancePtr disk_quota_ptr) override;
  void OnEnterpriseReportingInstanceReady(
      mojom::EnterpriseReportingInstancePtr enterprise_reporting_ptr) override;
  void OnFileSystemInstanceReady(
      mojom::FileSystemInstancePtr file_system_ptr) override;
  void OnImeInstanceReady(mojom::ImeInstancePtr ime_ptr) override;
  void OnInputMethodManagerInstanceReady(
      mojom::InputMethodManagerInstancePtr input_method_manager_ptr) override;
  void OnIntentHelperInstanceReady(
      mojom::IntentHelperInstancePtr intent_helper_ptr) override;
  void OnKeymasterInstanceReady(
      mojom::KeymasterInstancePtr keymaster_ptr) override;
  void OnKioskInstanceReady(mojom::KioskInstancePtr kiosk_ptr) override;
  void OnLockScreenInstanceReady(
      mojom::LockScreenInstancePtr lock_screen_ptr) override;
  void OnMediaSessionInstanceReady(
      mojom::MediaSessionInstancePtr media_session_ptr) override;
  void OnMetricsInstanceReady(mojom::MetricsInstancePtr metrics_ptr) override;
  void OnMidisInstanceReady(mojom::MidisInstancePtr midis_ptr) override;
  void OnNetInstanceReady(mojom::NetInstancePtr net_ptr) override;
  void OnNotificationsInstanceReady(
      mojom::NotificationsInstancePtr notifications_ptr) override;
  void OnObbMounterInstanceReady(
      mojom::ObbMounterInstancePtr obb_mounter_ptr) override;
  void OnOemCryptoInstanceReady(
      mojom::OemCryptoInstancePtr oemcrypto_ptr) override;
  void OnPipInstanceReady(mojom::PipInstancePtr pip_ptr) override;
  void OnPolicyInstanceReady(mojom::PolicyInstancePtr policy_ptr) override;
  void OnPowerInstanceReady(mojom::PowerInstancePtr power_ptr) override;
  void OnPrintInstanceReady(mojom::PrintInstancePtr print_ptr) override;
  void OnPrintSpoolerInstanceReady(
      mojom::PrintSpoolerInstancePtr print_spooler_ptr) override;
  void OnProcessInstanceReady(mojom::ProcessInstancePtr process_ptr) override;
  void OnPropertyInstanceReady(
      mojom::PropertyInstancePtr property_ptr) override;
  void OnRotationLockInstanceReady(
      mojom::RotationLockInstancePtr rotation_lock_ptr) override;
  void OnScreenCaptureInstanceReady(
      mojom::ScreenCaptureInstancePtr screen_capture_ptr) override;
  void OnSmartCardManagerInstanceReady(
      mojom::SmartCardManagerInstancePtr smart_card_manager_ptr) override;
  void OnStorageManagerInstanceReady(
      mojom::StorageManagerInstancePtr storage_manager_ptr) override;
  void OnTimerInstanceReady(mojom::TimerInstancePtr timer_ptr) override;
  void OnTracingInstanceReady(mojom::TracingInstancePtr trace_ptr) override;
  void OnTtsInstanceReady(mojom::TtsInstancePtr tts_ptr) override;
  void OnUsbHostInstanceReady(mojom::UsbHostInstancePtr usb_ptr) override;
  void OnVideoInstanceReady(mojom::VideoInstancePtr video_ptr) override;
  void OnVoiceInteractionArcHomeInstanceReady(
      mojom::VoiceInteractionArcHomeInstancePtr home_ptr) override;
  void OnVoiceInteractionFrameworkInstanceReady(
      mojom::VoiceInteractionFrameworkInstancePtr framework_ptr) override;
  void OnVolumeMounterInstanceReady(
      mojom::VolumeMounterInstancePtr volume_mounter_ptr) override;
  void OnWakeLockInstanceReady(
      mojom::WakeLockInstancePtr wakelock_ptr) override;
  void OnWallpaperInstanceReady(
      mojom::WallpaperInstancePtr wallpaper_ptr) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeArcBridgeHost);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_HOST_H_
