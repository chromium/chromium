// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/input_data_provider.h"

#include <fcntl.h>
#include <algorithm>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace chromeos {
namespace diagnostics {

namespace {

bool GetEventNodeId(base::FilePath path, int* id) {
  const std::string base_name_prefix = "event";

  std::string base_name = path.BaseName().value();
  DCHECK(base::StartsWith(base_name, base_name_prefix));
  base_name.erase(0, base_name_prefix.length());
  return base::StringToInt(base_name, id);
}

mojom::ConnectionType ConnectionTypeFromInputDeviceType(
    ui::InputDeviceType type) {
  switch (type) {
    case ui::InputDeviceType::INPUT_DEVICE_INTERNAL:
      return mojom::ConnectionType::kInternal;
    case ui::InputDeviceType::INPUT_DEVICE_USB:
      return mojom::ConnectionType::kUsb;
    case ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH:
      return mojom::ConnectionType::kBluetooth;
    case ui::InputDeviceType::INPUT_DEVICE_UNKNOWN:
      return mojom::ConnectionType::kUnknown;
  }
}

}  // namespace

InputDataProvider::InputDataProvider()
    : device_manager_(ui::CreateDeviceManager()) {
  Initialize();
}

InputDataProvider::InputDataProvider(
    std::unique_ptr<ui::DeviceManager> device_manager_for_test)
    : device_manager_(std::move(device_manager_for_test)) {
  Initialize();
}

InputDataProvider::~InputDataProvider() {
  device_manager_->RemoveObserver(this);
}

void InputDataProvider::Initialize() {
  device_manager_->AddObserver(this);
  device_manager_->ScanDevices(this);
}

void InputDataProvider::BindInterface(
    mojo::PendingReceiver<mojom::InputDataProvider> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void InputDataProvider::GetConnectedDevices(
    GetConnectedDevicesCallback callback) {
  std::vector<mojom::KeyboardInfoPtr> keyboard_vector;
  keyboard_vector.reserve(keyboards_.size());
  for (auto& keyboard_info : keyboards_) {
    keyboard_vector.push_back(keyboard_info.second.Clone());
  }

  std::vector<mojom::TouchDeviceInfoPtr> touch_device_vector;
  touch_device_vector.reserve(touch_devices_.size());
  for (auto& touch_device_info : touch_devices_) {
    touch_device_vector.push_back(touch_device_info.second.Clone());
  }

  base::ranges::sort(keyboard_vector, std::less<>(), &mojom::KeyboardInfo::id);
  base::ranges::sort(touch_device_vector, std::less<>(),
                     &mojom::TouchDeviceInfo::id);

  std::move(callback).Run(std::move(keyboard_vector),
                          std::move(touch_device_vector));
}

void InputDataProvider::OnDeviceEvent(const ui::DeviceEvent& event) {
  if (event.device_type() != ui::DeviceEvent::DeviceType::INPUT ||
      event.action_type() == ui::DeviceEvent::ActionType::CHANGE) {
    return;
  }

  int id = -1;
  if (!GetEventNodeId(event.path(), &id)) {
    LOG(ERROR) << "Ignoring DeviceEvent: invalid path " << event.path();
    return;
  }

  if (event.action_type() == ui::DeviceEvent::ActionType::ADD) {
    std::unique_ptr<ui::EventDeviceInfo> device_info =
        GetDeviceInfo(event.path());
    if (device_info == nullptr) {
      LOG(ERROR) << "Ignoring DeviceEvent for " << event.path();
      return;
    }

    if (device_info->HasTouchpad() ||
        (device_info->HasTouchscreen() && !device_info->HasStylus())) {
      AddTouchDevice(id, device_info.get());
    } else if (device_info->HasKeyboard()) {
      AddKeyboard(id, device_info.get());
    }
  } else {
    if (keyboards_.contains(id)) {
      keyboards_.erase(id);
    } else if (touch_devices_.contains(id)) {
      touch_devices_.erase(id);
    }
  }
}

std::unique_ptr<ui::EventDeviceInfo> InputDataProvider::GetDeviceInfo(
    base::FilePath path) {
  base::ScopedFD fd(open(path.value().c_str(), O_RDWR | O_NONBLOCK));
  if (fd.get() < 0) {
    LOG(ERROR) << "Couldn't open device path " << path;
    return nullptr;
  }

  auto device_info = std::make_unique<ui::EventDeviceInfo>();
  if (!device_info->Initialize(fd.get(), path)) {
    LOG(ERROR) << "Failed to get device info for " << path;
    return nullptr;
  }
  return device_info;
}

void InputDataProvider::AddTouchDevice(int id,
                                       const ui::EventDeviceInfo* device_info) {
  touch_devices_[id] = mojom::TouchDeviceInfo::New();
  touch_devices_[id]->id = id;
  touch_devices_[id]->connection_type =
      ConnectionTypeFromInputDeviceType(device_info->device_type());
  touch_devices_[id]->type = device_info->HasTouchpad()
                                 ? mojom::TouchDeviceType::kPointer
                                 : mojom::TouchDeviceType::kDirect;
  touch_devices_[id]->name = device_info->name();
}

void InputDataProvider::AddKeyboard(int id,
                                    const ui::EventDeviceInfo* device_info) {
  keyboards_[id] = mojom::KeyboardInfo::New();
  keyboards_[id]->id = id;
  keyboards_[id]->connection_type =
      ConnectionTypeFromInputDeviceType(device_info->device_type());
  keyboards_[id]->name = device_info->name();
}

}  // namespace diagnostics
}  // namespace chromeos
