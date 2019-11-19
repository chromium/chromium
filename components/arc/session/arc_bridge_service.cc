// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_bridge_service.h"

// These header is necessary for instantiation of ConnectionHolder.
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

ArcBridgeService::ArcBridgeService() = default;

ArcBridgeService::~ArcBridgeService() = default;

}  // namespace arc
