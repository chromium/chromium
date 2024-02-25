// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_touchpad_haptics.h"

#include <touchpad-haptics-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "components/exo/wayland/server_util.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace exo {
namespace wayland {
namespace {

class WaylandTouchpadHapticsDelegate {
 public:
  explicit WaylandTouchpadHapticsDelegate(wl_resource* resource)
      : resource_{resource} {}
  ~WaylandTouchpadHapticsDelegate() = default;

  void UpdateTouchpadHapticsState() {
    if (!base::FeatureList::IsEnabled(ash::features::kExoHapticFeedbackSupport))
      return;

    ui::InputController* controller =
        ui::OzonePlatform::GetInstance()->GetInputController();
    if (!controller) {
      LOG(ERROR) << "InputController is not available.";
      return;
    }
    if (last_activation_state_ &&
        *last_activation_state_ == controller->HasHapticTouchpad()) {
      // No need to send the update.
      return;
    }
    last_activation_state_ = controller->HasHapticTouchpad();
    if (*last_activation_state_)
      zcr_touchpad_haptics_v1_send_activated(resource_);
    else
      zcr_touchpad_haptics_v1_send_deactivated(resource_);
  }

  void Play(uint32_t effect, int32_t strength) {
    ui::InputController* controller =
        ui::OzonePlatform::GetInstance()->GetInputController();
    if (!controller) {
      LOG(ERROR) << "InputController is not available.";
      return;
    }
    controller->PlayHapticTouchpadEffect(
        static_cast<ui::HapticTouchpadEffect>(effect),
        static_cast<ui::HapticTouchpadEffectStrength>(strength));
  }

 private:
  const raw_ptr<wl_resource> resource_;
  std::optional<bool> last_activation_state_;
};

void touchpad_haptics_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void touchpad_haptics_play(wl_client* client,
                           wl_resource* resource,
                           uint32_t effect,
                           int32_t strength) {
  if (!base::FeatureList::IsEnabled(ash::features::kExoHapticFeedbackSupport))
    return;
  GetUserDataAs<WaylandTouchpadHapticsDelegate>(resource)->Play(effect,
                                                                strength);
}

const struct zcr_touchpad_haptics_v1_interface touchpad_haptics_implementation =
    {
        touchpad_haptics_destroy,
        touchpad_haptics_play,
};

}  // namespace

void bind_touchpad_haptics(wl_client* client,
                           void* data,
                           uint32_t version,
                           uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zcr_touchpad_haptics_v1_interface, version, id);

  SetImplementation(resource, &touchpad_haptics_implementation,
                    std::make_unique<WaylandTouchpadHapticsDelegate>(resource));
  GetUserDataAs<WaylandTouchpadHapticsDelegate>(resource)
      ->UpdateTouchpadHapticsState();
}

}  // namespace wayland
}  // namespace exo
