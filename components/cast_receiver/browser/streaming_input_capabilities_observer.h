// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_INPUT_CAPABILITIES_OBSERVER_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_INPUT_CAPABILITIES_OBSERVER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "components/cast_receiver/proto/input_capabilities.pb.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ui {
class DeviceDataManager;
}

namespace cast_receiver {

// Observes input device configuration changes (hotplugs) via
// ui::DeviceDataManager and reports the updated InputCapabilities proto.
class StreamingInputCapabilitiesObserver : public ui::InputDeviceEventObserver {
 public:
  using InputCapabilitiesCallback =
      base::RepeatingCallback<void(const cast_receiver::InputCapabilities&)>;

  // Registers itself as an observer to the non-null DeviceDataManager.
  // |callback| will be invoked with the initial capabilities if the device
  // lists are already complete, and subsequently whenever they change.
  StreamingInputCapabilitiesObserver(ui::DeviceDataManager* device_data_manager,
                                     InputCapabilitiesCallback callback);
  ~StreamingInputCapabilitiesObserver() override;

  StreamingInputCapabilitiesObserver(
      const StreamingInputCapabilitiesObserver&) = delete;
  StreamingInputCapabilitiesObserver& operator=(
      const StreamingInputCapabilitiesObserver&) = delete;

 private:
  // ui::InputDeviceEventObserver implementation:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;
  void OnDeviceListsComplete() override;

  void UpdateCapabilities();
  cast_receiver::InputCapabilities BuildCapabilities() const;

  ui::DeviceDataManager* const device_data_manager_;
  InputCapabilitiesCallback callback_;
  std::optional<std::string> last_serialized_capabilities_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_INPUT_CAPABILITIES_OBSERVER_H_
