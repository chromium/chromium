// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/session/arc_bridge_service.h"

// These header is necessary for instantiation of ConnectionHolder.
#include "chromeos/ash/experiences/arc/mojom/adbd.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/app.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/app_permissions.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/appfuse.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/arc_bridge.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/arc_wifi.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/audio.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/auth.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/backup_settings.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/bluetooth.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/boot_phase_monitor.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/camera.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/crash_collector.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/crosh.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/digital_goods.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/disk_space.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/enterprise_reporting.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/error_notification.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/file_system.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/iio_sensor.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/ime.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/input_method_manager.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/intent_helper.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/keymaster.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/keymint.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/kiosk.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/media_session.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/metrics.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/midis.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/nearby_share.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/net.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/obb_mounter.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/oemcrypto.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/on_device_safety.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/pip.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/policy.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/power.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/print_spooler.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/privacy_items.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/process.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/screen_capture.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/sharesheet.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/system_state.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/timer.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/tracing.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/tts.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/usb_host.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/video.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/volume_mounter.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/wake_lock.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/wallpaper.mojom.h"
#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom.h"

namespace arc {

ArcBridgeService::ArcBridgeService() = default;

ArcBridgeService::~ArcBridgeService() = default;

void ArcBridgeService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcBridgeService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ArcBridgeService::ObserveBeforeArcBridgeClosed() {
  for (auto& observer : observer_list_) {
    observer.BeforeArcBridgeClosed();
  }
}

void ArcBridgeService::ObserveAfterArcBridgeClosed() {
  for (auto& observer : observer_list_) {
    observer.AfterArcBridgeClosed();
  }
}

}  // namespace arc
