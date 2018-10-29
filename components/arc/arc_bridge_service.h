// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_
#define COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_

#include "base/macros.h"
#include "components/arc/connection_holder.h"

namespace arc {

namespace mojom {

// Instead of including components/arc/common/arc_bridge.mojom.h, list all the
// instance classes here for faster build.
class AccessibilityHelperHost;
class AccessibilityHelperInstance;
class AppHost;
class AppInstance;
class AppfuseHost;
class AppfuseInstance;
class AudioHost;
class AudioInstance;
class AuthHost;
class AuthInstance;
class BackupSettingsInstance;
class BluetoothHost;
class BluetoothInstance;
class BootPhaseMonitorHost;
class BootPhaseMonitorInstance;
class CastReceiverInstance;
class CertStoreHost;
class CertStoreInstance;
class ClipboardHost;
class ClipboardInstance;
class CrashCollectorHost;
class CrashCollectorInstance;
class DiskQuotaHost;
class DiskQuotaInstance;
class EnterpriseReportingHost;
class EnterpriseReportingInstance;
class FileSystemHost;
class FileSystemInstance;
class ImeHost;
class ImeInstance;
class InputMethodManagerHost;
class InputMethodManagerInstance;
class IntentHelperHost;
class IntentHelperInstance;
class KioskHost;
class KioskInstance;
class LockScreenInstance;
class MediaSessionInstance;
class MetricsHost;
class MetricsInstance;
class MidisHost;
class MidisInstance;
class NetHost;
class NetInstance;
class ObbMounterHost;
class ObbMounterInstance;
class OemCryptoHost;
class OemCryptoInstance;
class PipHost;
class PipInstance;
class PolicyHost;
class PolicyInstance;
class PowerHost;
class PowerInstance;
class PrintHost;
class PrintInstance;
class ProcessInstance;
class PropertyInstance;
class RotationLockInstance;
class ScreenCaptureHost;
class ScreenCaptureInstance;
class StorageManagerInstance;
class TimerHost;
class TimerInstance;
class TracingInstance;
class TtsHost;
class TtsInstance;
class UsbHostHost;
class UsbHostInstance;
class VideoHost;
class VideoInstance;
class VoiceInteractionArcHomeHost;
class VoiceInteractionArcHomeInstance;
class VoiceInteractionFrameworkHost;
class VoiceInteractionFrameworkInstance;
class VolumeMounterHost;
class VolumeMounterInstance;
class WakeLockHost;
class WakeLockInstance;
class WallpaperHost;
class WallpaperInstance;

}  // namespace mojom

// Holds Mojo channels which proxy to ARC side implementation. The actual
// instances are set/removed via ArcBridgeHostImpl.
class ArcBridgeService {
 public:
  ArcBridgeService();
  ~ArcBridgeService();

  ConnectionHolder<mojom::AccessibilityHelperInstance,
                   mojom::AccessibilityHelperHost>*
  accessibility_helper() {
    return &accessibility_helper_;
  }
  ConnectionHolder<mojom::AppInstance, mojom::AppHost>* app() { return &app_; }
  ConnectionHolder<mojom::AppfuseInstance, mojom::AppfuseHost>* appfuse() {
    return &appfuse_;
  }
  ConnectionHolder<mojom::AudioInstance, mojom::AudioHost>* audio() {
    return &audio_;
  }
  ConnectionHolder<mojom::AuthInstance, mojom::AuthHost>* auth() {
    return &auth_;
  }
  ConnectionHolder<mojom::BackupSettingsInstance>* backup_settings() {
    return &backup_settings_;
  }
  ConnectionHolder<mojom::BluetoothInstance, mojom::BluetoothHost>*
  bluetooth() {
    return &bluetooth_;
  }
  ConnectionHolder<mojom::BootPhaseMonitorInstance,
                   mojom::BootPhaseMonitorHost>*
  boot_phase_monitor() {
    return &boot_phase_monitor_;
  }
  ConnectionHolder<mojom::CastReceiverInstance>* cast_receiver() {
    return &cast_receiver_;
  }
  ConnectionHolder<mojom::CertStoreInstance, mojom::CertStoreHost>*
  cert_store() {
    return &cert_store_;
  }
  ConnectionHolder<mojom::ClipboardInstance, mojom::ClipboardHost>*
  clipboard() {
    return &clipboard_;
  }
  ConnectionHolder<mojom::CrashCollectorInstance, mojom::CrashCollectorHost>*
  crash_collector() {
    return &crash_collector_;
  }
  ConnectionHolder<mojom::DiskQuotaInstance, mojom::DiskQuotaHost>*
  disk_quota() {
    return &disk_quota_;
  }
  ConnectionHolder<mojom::EnterpriseReportingInstance,
                   mojom::EnterpriseReportingHost>*
  enterprise_reporting() {
    return &enterprise_reporting_;
  }
  ConnectionHolder<mojom::FileSystemInstance, mojom::FileSystemHost>*
  file_system() {
    return &file_system_;
  }
  ConnectionHolder<mojom::ImeInstance, mojom::ImeHost>* ime() { return &ime_; }
  ConnectionHolder<mojom::InputMethodManagerInstance,
                   mojom::InputMethodManagerHost>*
  input_method_manager() {
    return &input_method_manager_;
  }
  ConnectionHolder<mojom::IntentHelperInstance, mojom::IntentHelperHost>*
  intent_helper() {
    return &intent_helper_;
  }
  ConnectionHolder<mojom::KioskInstance, mojom::KioskHost>* kiosk() {
    return &kiosk_;
  }
  ConnectionHolder<mojom::LockScreenInstance>* lock_screen() {
    return &lock_screen_;
  }
  ConnectionHolder<mojom::MediaSessionInstance>* media_session() {
    return &media_session_;
  }
  ConnectionHolder<mojom::MetricsInstance, mojom::MetricsHost>* metrics() {
    return &metrics_;
  }
  ConnectionHolder<mojom::MidisInstance, mojom::MidisHost>* midis() {
    return &midis_;
  }
  ConnectionHolder<mojom::NetInstance, mojom::NetHost>* net() { return &net_; }
  ConnectionHolder<mojom::ObbMounterInstance, mojom::ObbMounterHost>*
  obb_mounter() {
    return &obb_mounter_;
  }
  ConnectionHolder<mojom::OemCryptoInstance, mojom::OemCryptoHost>*
  oemcrypto() {
    return &oemcrypto_;
  }
  ConnectionHolder<mojom::PipInstance, mojom::PipHost>* pip() { return &pip_; }
  ConnectionHolder<mojom::PolicyInstance, mojom::PolicyHost>* policy() {
    return &policy_;
  }
  ConnectionHolder<mojom::PowerInstance, mojom::PowerHost>* power() {
    return &power_;
  }
  ConnectionHolder<mojom::PrintInstance, mojom::PrintHost>* print() {
    return &print_;
  }
  ConnectionHolder<mojom::ProcessInstance>* process() { return &process_; }
  ConnectionHolder<mojom::PropertyInstance>* property() { return &property_; }
  ConnectionHolder<mojom::RotationLockInstance>* rotation_lock() {
    return &rotation_lock_;
  }
  ConnectionHolder<mojom::ScreenCaptureInstance, mojom::ScreenCaptureHost>*
  screen_capture() {
    return &screen_capture_;
  }
  ConnectionHolder<mojom::StorageManagerInstance>* storage_manager() {
    return &storage_manager_;
  }
  ConnectionHolder<mojom::TimerInstance, mojom::TimerHost>* timer() {
    return &timer_;
  }
  ConnectionHolder<mojom::TracingInstance>* tracing() { return &tracing_; }
  ConnectionHolder<mojom::TtsInstance, mojom::TtsHost>* tts() { return &tts_; }
  ConnectionHolder<mojom::UsbHostInstance, mojom::UsbHostHost>* usb_host() {
    return &usb_host_;
  }
  ConnectionHolder<mojom::VideoInstance, mojom::VideoHost>* video() {
    return &video_;
  }
  ConnectionHolder<mojom::VoiceInteractionArcHomeInstance,
                   mojom::VoiceInteractionArcHomeHost>*
  voice_interaction_arc_home() {
    return &voice_interaction_arc_home_;
  }
  ConnectionHolder<mojom::VoiceInteractionFrameworkInstance,
                   mojom::VoiceInteractionFrameworkHost>*
  voice_interaction_framework() {
    return &voice_interaction_framework_;
  }
  ConnectionHolder<mojom::VolumeMounterInstance, mojom::VolumeMounterHost>*
  volume_mounter() {
    return &volume_mounter_;
  }
  ConnectionHolder<mojom::WakeLockInstance, mojom::WakeLockHost>* wake_lock() {
    return &wake_lock_;
  }
  ConnectionHolder<mojom::WallpaperInstance, mojom::WallpaperHost>*
  wallpaper() {
    return &wallpaper_;
  }

 private:
  ConnectionHolder<mojom::AccessibilityHelperInstance,
                   mojom::AccessibilityHelperHost>
      accessibility_helper_;
  ConnectionHolder<mojom::AppInstance, mojom::AppHost> app_;
  ConnectionHolder<mojom::AppfuseInstance, mojom::AppfuseHost> appfuse_;
  ConnectionHolder<mojom::AudioInstance, mojom::AudioHost> audio_;
  ConnectionHolder<mojom::AuthInstance, mojom::AuthHost> auth_;
  ConnectionHolder<mojom::BackupSettingsInstance> backup_settings_;
  ConnectionHolder<mojom::BluetoothInstance, mojom::BluetoothHost> bluetooth_;
  ConnectionHolder<mojom::BootPhaseMonitorInstance, mojom::BootPhaseMonitorHost>
      boot_phase_monitor_;
  ConnectionHolder<mojom::CastReceiverInstance> cast_receiver_;
  ConnectionHolder<mojom::CertStoreInstance, mojom::CertStoreHost> cert_store_;
  ConnectionHolder<mojom::ClipboardInstance, mojom::ClipboardHost> clipboard_;
  ConnectionHolder<mojom::CrashCollectorInstance, mojom::CrashCollectorHost>
      crash_collector_;
  ConnectionHolder<mojom::DiskQuotaInstance, mojom::DiskQuotaHost> disk_quota_;
  ConnectionHolder<mojom::EnterpriseReportingInstance,
                   mojom::EnterpriseReportingHost>
      enterprise_reporting_;
  ConnectionHolder<mojom::FileSystemInstance, mojom::FileSystemHost>
      file_system_;
  ConnectionHolder<mojom::ImeInstance, mojom::ImeHost> ime_;
  ConnectionHolder<mojom::InputMethodManagerInstance,
                   mojom::InputMethodManagerHost>
      input_method_manager_;
  ConnectionHolder<mojom::IntentHelperInstance, mojom::IntentHelperHost>
      intent_helper_;
  ConnectionHolder<mojom::KioskInstance, mojom::KioskHost> kiosk_;
  ConnectionHolder<mojom::LockScreenInstance> lock_screen_;
  ConnectionHolder<mojom::MediaSessionInstance> media_session_;
  ConnectionHolder<mojom::MetricsInstance, mojom::MetricsHost> metrics_;
  ConnectionHolder<mojom::MidisInstance, mojom::MidisHost> midis_;
  ConnectionHolder<mojom::NetInstance, mojom::NetHost> net_;
  ConnectionHolder<mojom::ObbMounterInstance, mojom::ObbMounterHost>
      obb_mounter_;
  ConnectionHolder<mojom::OemCryptoInstance, mojom::OemCryptoHost> oemcrypto_;
  ConnectionHolder<mojom::PipInstance, mojom::PipHost> pip_;
  ConnectionHolder<mojom::PolicyInstance, mojom::PolicyHost> policy_;
  ConnectionHolder<mojom::PowerInstance, mojom::PowerHost> power_;
  ConnectionHolder<mojom::PrintInstance, mojom::PrintHost> print_;
  ConnectionHolder<mojom::ProcessInstance> process_;
  ConnectionHolder<mojom::PropertyInstance> property_;
  ConnectionHolder<mojom::RotationLockInstance> rotation_lock_;
  ConnectionHolder<mojom::ScreenCaptureInstance, mojom::ScreenCaptureHost>
      screen_capture_;
  ConnectionHolder<mojom::StorageManagerInstance> storage_manager_;
  ConnectionHolder<mojom::TimerInstance, mojom::TimerHost> timer_;
  ConnectionHolder<mojom::TracingInstance> tracing_;
  ConnectionHolder<mojom::TtsInstance, mojom::TtsHost> tts_;
  ConnectionHolder<mojom::UsbHostInstance, mojom::UsbHostHost> usb_host_;
  ConnectionHolder<mojom::VideoInstance, mojom::VideoHost> video_;
  ConnectionHolder<mojom::VoiceInteractionArcHomeInstance,
                   mojom::VoiceInteractionArcHomeHost>
      voice_interaction_arc_home_;
  ConnectionHolder<mojom::VoiceInteractionFrameworkInstance,
                   mojom::VoiceInteractionFrameworkHost>
      voice_interaction_framework_;
  ConnectionHolder<mojom::VolumeMounterInstance, mojom::VolumeMounterHost>
      volume_mounter_;
  ConnectionHolder<mojom::WakeLockInstance, mojom::WakeLockHost> wake_lock_;
  ConnectionHolder<mojom::WallpaperInstance, mojom::WallpaperHost> wallpaper_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_
