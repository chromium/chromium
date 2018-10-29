// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_host_impl.h"

#include <algorithm>
#include <utility>

#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/mojo_channel.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace arc {

ArcBridgeHostImpl::ArcBridgeHostImpl(ArcBridgeService* arc_bridge_service,
                                     mojom::ArcBridgeInstancePtr instance)
    : arc_bridge_service_(arc_bridge_service),
      binding_(this),
      instance_(std::move(instance)) {
  DCHECK(arc_bridge_service_);
  DCHECK(instance_.is_bound());
  instance_.set_connection_error_handler(
      base::BindOnce(&ArcBridgeHostImpl::OnClosed, base::Unretained(this)));
  mojom::ArcBridgeHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  instance_->Init(std::move(host_proxy));
}

ArcBridgeHostImpl::~ArcBridgeHostImpl() {
  OnClosed();
}

void ArcBridgeHostImpl::OnAccessibilityHelperInstanceReady(
    mojom::AccessibilityHelperInstancePtr accessibility_helper_ptr) {
  OnInstanceReady(arc_bridge_service_->accessibility_helper(),
                  std::move(accessibility_helper_ptr));
}

void ArcBridgeHostImpl::OnAppInstanceReady(mojom::AppInstancePtr app_ptr) {
  OnInstanceReady(arc_bridge_service_->app(), std::move(app_ptr));
}

void ArcBridgeHostImpl::OnAppfuseInstanceReady(
    mojom::AppfuseInstancePtr appfuse_ptr) {
  OnInstanceReady(arc_bridge_service_->appfuse(), std::move(appfuse_ptr));
}

void ArcBridgeHostImpl::OnAudioInstanceReady(
    mojom::AudioInstancePtr audio_ptr) {
  OnInstanceReady(arc_bridge_service_->audio(), std::move(audio_ptr));
}

void ArcBridgeHostImpl::OnAuthInstanceReady(mojom::AuthInstancePtr auth_ptr) {
  OnInstanceReady(arc_bridge_service_->auth(), std::move(auth_ptr));
}

void ArcBridgeHostImpl::OnBackupSettingsInstanceReady(
    mojom::BackupSettingsInstancePtr backup_settings_ptr) {
  OnInstanceReady(arc_bridge_service_->backup_settings(),
                  std::move(backup_settings_ptr));
}

void ArcBridgeHostImpl::OnBluetoothInstanceReady(
    mojom::BluetoothInstancePtr bluetooth_ptr) {
  OnInstanceReady(arc_bridge_service_->bluetooth(), std::move(bluetooth_ptr));
}

void ArcBridgeHostImpl::OnBootPhaseMonitorInstanceReady(
    mojom::BootPhaseMonitorInstancePtr boot_phase_monitor_ptr) {
  OnInstanceReady(arc_bridge_service_->boot_phase_monitor(),
                  std::move(boot_phase_monitor_ptr));
}

void ArcBridgeHostImpl::OnCastReceiverInstanceReady(
    mojom::CastReceiverInstancePtr cast_receiver_ptr) {
  OnInstanceReady(arc_bridge_service_->cast_receiver(),
                  std::move(cast_receiver_ptr));
}

void ArcBridgeHostImpl::OnCertStoreInstanceReady(
    mojom::CertStoreInstancePtr instance_ptr) {
  OnInstanceReady(arc_bridge_service_->cert_store(), std::move(instance_ptr));
}

void ArcBridgeHostImpl::OnClipboardInstanceReady(
    mojom::ClipboardInstancePtr clipboard_ptr) {
  OnInstanceReady(arc_bridge_service_->clipboard(), std::move(clipboard_ptr));
}

void ArcBridgeHostImpl::OnCrashCollectorInstanceReady(
    mojom::CrashCollectorInstancePtr crash_collector_ptr) {
  OnInstanceReady(arc_bridge_service_->crash_collector(),
                  std::move(crash_collector_ptr));
}

void ArcBridgeHostImpl::OnDiskQuotaInstanceReady(
    mojom::DiskQuotaInstancePtr disk_quota_ptr) {
  OnInstanceReady(arc_bridge_service_->disk_quota(), std::move(disk_quota_ptr));
}

void ArcBridgeHostImpl::OnEnterpriseReportingInstanceReady(
    mojom::EnterpriseReportingInstancePtr enterprise_reporting_ptr) {
  OnInstanceReady(arc_bridge_service_->enterprise_reporting(),
                  std::move(enterprise_reporting_ptr));
}

void ArcBridgeHostImpl::OnFileSystemInstanceReady(
    mojom::FileSystemInstancePtr file_system_ptr) {
  OnInstanceReady(arc_bridge_service_->file_system(),
                  std::move(file_system_ptr));
}

void ArcBridgeHostImpl::OnImeInstanceReady(mojom::ImeInstancePtr ime_ptr) {
  OnInstanceReady(arc_bridge_service_->ime(), std::move(ime_ptr));
}

void ArcBridgeHostImpl::OnInputMethodManagerInstanceReady(
    mojom::InputMethodManagerInstancePtr input_method_manager_ptr) {
  OnInstanceReady(arc_bridge_service_->input_method_manager(),
                  std::move(input_method_manager_ptr));
}

void ArcBridgeHostImpl::OnIntentHelperInstanceReady(
    mojom::IntentHelperInstancePtr intent_helper_ptr) {
  OnInstanceReady(arc_bridge_service_->intent_helper(),
                  std::move(intent_helper_ptr));
}

void ArcBridgeHostImpl::OnKioskInstanceReady(
    mojom::KioskInstancePtr kiosk_ptr) {
  OnInstanceReady(arc_bridge_service_->kiosk(), std::move(kiosk_ptr));
}

void ArcBridgeHostImpl::OnLockScreenInstanceReady(
    mojom::LockScreenInstancePtr lock_screen_ptr) {
  OnInstanceReady(arc_bridge_service_->lock_screen(),
                  std::move(lock_screen_ptr));
}

void ArcBridgeHostImpl::OnMediaSessionInstanceReady(
    mojom::MediaSessionInstancePtr media_session_ptr) {
  OnInstanceReady(arc_bridge_service_->media_session(),
                  std::move(media_session_ptr));
}

void ArcBridgeHostImpl::OnMetricsInstanceReady(
    mojom::MetricsInstancePtr metrics_ptr) {
  OnInstanceReady(arc_bridge_service_->metrics(), std::move(metrics_ptr));
}

void ArcBridgeHostImpl::OnMidisInstanceReady(
    mojom::MidisInstancePtr midis_ptr) {
  OnInstanceReady(arc_bridge_service_->midis(), std::move(midis_ptr));
}

void ArcBridgeHostImpl::OnNetInstanceReady(mojom::NetInstancePtr net_ptr) {
  OnInstanceReady(arc_bridge_service_->net(), std::move(net_ptr));
}

void ArcBridgeHostImpl::OnNotificationsInstanceReady(
    mojom::NotificationsInstancePtr notifications_ptr) {
  // Forward notification instance to ash.
  ash::mojom::AshMessageCenterControllerPtr ash_message_center_controller;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &ash_message_center_controller);
  ash_message_center_controller->SetArcNotificationsInstance(
      std::move(notifications_ptr));
}

void ArcBridgeHostImpl::OnObbMounterInstanceReady(
    mojom::ObbMounterInstancePtr obb_mounter_ptr) {
  OnInstanceReady(arc_bridge_service_->obb_mounter(),
                  std::move(obb_mounter_ptr));
}

void ArcBridgeHostImpl::OnOemCryptoInstanceReady(
    mojom::OemCryptoInstancePtr oemcrypto_ptr) {
  OnInstanceReady(arc_bridge_service_->oemcrypto(), std::move(oemcrypto_ptr));
}

void ArcBridgeHostImpl::OnPipInstanceReady(mojom::PipInstancePtr pip_ptr) {
  OnInstanceReady(arc_bridge_service_->pip(), std::move(pip_ptr));
}

void ArcBridgeHostImpl::OnPolicyInstanceReady(
    mojom::PolicyInstancePtr policy_ptr) {
  OnInstanceReady(arc_bridge_service_->policy(), std::move(policy_ptr));
}

void ArcBridgeHostImpl::OnPowerInstanceReady(
    mojom::PowerInstancePtr power_ptr) {
  OnInstanceReady(arc_bridge_service_->power(), std::move(power_ptr));
}

void ArcBridgeHostImpl::OnPrintInstanceReady(
    mojom::PrintInstancePtr print_ptr) {
  OnInstanceReady(arc_bridge_service_->print(), std::move(print_ptr));
}

void ArcBridgeHostImpl::OnProcessInstanceReady(
    mojom::ProcessInstancePtr process_ptr) {
  OnInstanceReady(arc_bridge_service_->process(), std::move(process_ptr));
}

void ArcBridgeHostImpl::OnPropertyInstanceReady(
    mojom::PropertyInstancePtr property_ptr) {
  OnInstanceReady(arc_bridge_service_->property(), std::move(property_ptr));
}

void ArcBridgeHostImpl::OnRotationLockInstanceReady(
    mojom::RotationLockInstancePtr rotation_lock_ptr) {
  OnInstanceReady(arc_bridge_service_->rotation_lock(),
                  std::move(rotation_lock_ptr));
}

void ArcBridgeHostImpl::OnScreenCaptureInstanceReady(
    mojom::ScreenCaptureInstancePtr screen_capture_ptr) {
  OnInstanceReady(arc_bridge_service_->screen_capture(),
                  std::move(screen_capture_ptr));
}

void ArcBridgeHostImpl::OnStorageManagerInstanceReady(
    mojom::StorageManagerInstancePtr storage_manager_ptr) {
  OnInstanceReady(arc_bridge_service_->storage_manager(),
                  std::move(storage_manager_ptr));
}

void ArcBridgeHostImpl::OnTimerInstanceReady(
    mojom::TimerInstancePtr timer_ptr) {
  OnInstanceReady(arc_bridge_service_->timer(), std::move(timer_ptr));
}

void ArcBridgeHostImpl::OnTracingInstanceReady(
    mojom::TracingInstancePtr tracing_ptr) {
  OnInstanceReady(arc_bridge_service_->tracing(), std::move(tracing_ptr));
}

void ArcBridgeHostImpl::OnTtsInstanceReady(mojom::TtsInstancePtr tts_ptr) {
  OnInstanceReady(arc_bridge_service_->tts(), std::move(tts_ptr));
}

void ArcBridgeHostImpl::OnUsbHostInstanceReady(
    mojom::UsbHostInstancePtr usb_host_ptr) {
  OnInstanceReady(arc_bridge_service_->usb_host(), std::move(usb_host_ptr));
}

void ArcBridgeHostImpl::OnVideoInstanceReady(
    mojom::VideoInstancePtr video_ptr) {
  OnInstanceReady(arc_bridge_service_->video(), std::move(video_ptr));
}

void ArcBridgeHostImpl::OnVoiceInteractionArcHomeInstanceReady(
    mojom::VoiceInteractionArcHomeInstancePtr home_ptr) {
  OnInstanceReady(arc_bridge_service_->voice_interaction_arc_home(),
                  std::move(home_ptr));
}

void ArcBridgeHostImpl::OnVoiceInteractionFrameworkInstanceReady(
    mojom::VoiceInteractionFrameworkInstancePtr framework_ptr) {
  OnInstanceReady(arc_bridge_service_->voice_interaction_framework(),
                  std::move(framework_ptr));
}

void ArcBridgeHostImpl::OnVolumeMounterInstanceReady(
    mojom::VolumeMounterInstancePtr volume_mounter_ptr) {
  OnInstanceReady(arc_bridge_service_->volume_mounter(),
                  std::move(volume_mounter_ptr));
}

void ArcBridgeHostImpl::OnWakeLockInstanceReady(
    mojom::WakeLockInstancePtr wakelock_ptr) {
  OnInstanceReady(arc_bridge_service_->wake_lock(), std::move(wakelock_ptr));
}

void ArcBridgeHostImpl::OnWallpaperInstanceReady(
    mojom::WallpaperInstancePtr wallpaper_ptr) {
  OnInstanceReady(arc_bridge_service_->wallpaper(), std::move(wallpaper_ptr));
}

void ArcBridgeHostImpl::OnClosed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(1) << "Mojo connection lost";

  // Close all mojo channels.
  mojo_channels_.clear();
  instance_.reset();
  if (binding_.is_bound())
    binding_.Close();
}

template <typename InstanceType, typename HostType>
void ArcBridgeHostImpl::OnInstanceReady(
    ConnectionHolder<InstanceType, HostType>* holder,
    mojo::InterfacePtr<InstanceType> ptr) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(binding_.is_bound());
  DCHECK(ptr.is_bound());

  // Track |channel|'s lifetime via |mojo_channels_| so that it will be
  // closed on ArcBridgeHost/Instance closing or the ArcBridgeHostImpl's
  // destruction.
  auto* channel =
      new MojoChannel<InstanceType, HostType>(holder, std::move(ptr));
  mojo_channels_.emplace_back(channel);

  // Since |channel| is managed by |mojo_channels_|, its lifetime is shorter
  // than |this|. Thus, the connection error handler will be invoked only
  // when |this| is alive and base::Unretained is safe here.
  channel->set_connection_error_handler(base::BindOnce(
      &ArcBridgeHostImpl::OnChannelClosed, base::Unretained(this), channel));

  // Call QueryVersion so that the version info is properly stored in the
  // InterfacePtr<T>.
  channel->QueryVersion();
}

void ArcBridgeHostImpl::OnChannelClosed(MojoChannelBase* channel) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  mojo_channels_.erase(
      std::find_if(mojo_channels_.begin(), mojo_channels_.end(),
                   [channel](std::unique_ptr<MojoChannelBase>& ptr) {
                     return ptr.get() == channel;
                   }));
}

}  // namespace arc
