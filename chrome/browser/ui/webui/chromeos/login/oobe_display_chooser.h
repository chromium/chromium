// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_DISPLAY_CHOOSER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_DISPLAY_CHOOSER_H_

#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ui {
class DeviceDataManager;
}

namespace chromeos {

class OobeDisplayChooser : public ui::InputDeviceEventObserver {
 public:
  OobeDisplayChooser();
  ~OobeDisplayChooser() override;

  // Tries to put the OOBE UI on a connected touch display (if available).
  // Must be called on the BrowserThread::UI thread.
  void TryToPlaceUiOnTouchDisplay();

  void set_cros_display_config_for_test(
      mojo::PendingRemote<ash::mojom::CrosDisplayConfigController>
          cros_display_config) {
    cros_display_config_.Bind(std::move(cros_display_config));
  }

 private:
  // Calls MoveToTouchDisplay() if touch device list is ready, otherwise adds an
  // observer that calls MoveToTouchDisplay() once ready.
  void MaybeMoveToTouchDisplay();

  void MoveToTouchDisplay();

  // ui::InputDeviceEventObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;
  void OnTouchDeviceAssociationChanged() override;
  void OnDeviceListsComplete() override;

  ScopedObserver<ui::DeviceDataManager, ui::InputDeviceEventObserver>
      scoped_observer_{this};
  mojo::Remote<ash::mojom::CrosDisplayConfigController> cros_display_config_;

  base::WeakPtrFactory<OobeDisplayChooser> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OobeDisplayChooser);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_DISPLAY_CHOOSER_H_
