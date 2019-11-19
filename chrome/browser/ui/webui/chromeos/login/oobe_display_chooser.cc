// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/oobe_display_chooser.h"

#include <stdint.h>

#include "ash/public/mojom/constants.mojom.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/touchscreen_device.h"

using content::BrowserThread;

namespace chromeos {

namespace {

bool TouchSupportAvailable(const display::Display& display) {
  return display.touch_support() == display::Display::TouchSupport::AVAILABLE;
}

// TODO(felixe): More context at crbug.com/738885
const uint16_t kDeviceIds[] = {0x0457, 0x266e, 0x222a};

// Returns true if |vendor_id| is a valid vendor id that may be made the primary
// display.
bool IsWhiteListedVendorId(uint16_t vendor_id) {
  return base::Contains(kDeviceIds, vendor_id);
}

}  // namespace

OobeDisplayChooser::OobeDisplayChooser() {
  // |connector| may be null in tests.
  auto* connector = content::GetSystemConnector();
  if (connector) {
    connector->Connect(ash::mojom::kServiceName,
                       cros_display_config_.BindNewPipeAndPassReceiver());
  }
}

OobeDisplayChooser::~OobeDisplayChooser() {}

void OobeDisplayChooser::TryToPlaceUiOnTouchDisplay() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Don't (potentially) queue a second task to run MoveToTouchDisplay if one
  // already is queued.
  if (weak_ptr_factory_.HasWeakPtrs())
    return;

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  if (primary_display.is_valid() && !TouchSupportAvailable(primary_display)) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&OobeDisplayChooser::MaybeMoveToTouchDisplay,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void OobeDisplayChooser::MaybeMoveToTouchDisplay() {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  if (device_data_manager->AreDeviceListsComplete() &&
      device_data_manager->AreTouchscreenTargetDisplaysValid()) {
    MoveToTouchDisplay();
  } else if (!scoped_observer_.IsObserving(device_data_manager)) {
    scoped_observer_.Add(device_data_manager);
  }
}

void OobeDisplayChooser::MoveToTouchDisplay() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_observer_.RemoveAll();

  const ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  for (const ui::TouchscreenDevice& device :
       device_data_manager->GetTouchscreenDevices()) {
    if (IsWhiteListedVendorId(device.vendor_id) &&
        device.target_display_id != display::kInvalidDisplayId) {
      auto config_properties = ash::mojom::DisplayConfigProperties::New();
      config_properties->set_primary = true;
      cros_display_config_->SetDisplayProperties(
          base::NumberToString(device.target_display_id),
          std::move(config_properties), ash::mojom::DisplayConfigSource::kUser,
          base::DoNothing());
      break;
    }
  }
}

void OobeDisplayChooser::OnTouchDeviceAssociationChanged() {
  MaybeMoveToTouchDisplay();
}

void OobeDisplayChooser::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & ui::InputDeviceEventObserver::kTouchscreen) {
    MaybeMoveToTouchDisplay();
  }
}

void OobeDisplayChooser::OnDeviceListsComplete() {
  MaybeMoveToTouchDisplay();
}

}  // namespace chromeos
