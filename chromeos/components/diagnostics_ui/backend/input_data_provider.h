// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "chromeos/components/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace chromeos {
namespace diagnostics {

class InputDataProvider : public mojom::InputDataProvider,
                          public ui::DeviceEventObserver {
 public:
  InputDataProvider();
  InputDataProvider(std::unique_ptr<ui::DeviceManager> device_manager);
  InputDataProvider(const InputDataProvider&) = delete;
  InputDataProvider& operator=(const InputDataProvider&) = delete;
  ~InputDataProvider() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::InputDataProvider> pending_receiver);

  // mojom::InputDataProvider:
  void GetConnectedDevices(GetConnectedDevicesCallback callback) override;

  // ui::DeviceEventObserver:
  void OnDeviceEvent(const ui::DeviceEvent& event) override;

 protected:
  virtual std::unique_ptr<ui::EventDeviceInfo> GetDeviceInfo(
      base::FilePath path);

 private:
  void Initialize();

  void AddTouchDevice(int id, const ui::EventDeviceInfo* device_info);
  void AddKeyboard(int id, const ui::EventDeviceInfo* device_info);

  base::flat_map<int, mojom::KeyboardInfoPtr> keyboards_;
  base::flat_map<int, mojom::TouchDeviceInfoPtr> touch_devices_;

  mojo::Receiver<mojom::InputDataProvider> receiver_{this};

  std::unique_ptr<ui::DeviceManager> device_manager_;
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_DATA_PROVIDER_H_
