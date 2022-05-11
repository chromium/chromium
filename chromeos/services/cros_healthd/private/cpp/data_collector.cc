// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/private/cpp/data_collector.h"

#include <fcntl.h>

#include "base/check_op.h"
#include "base/files/file_enumerator.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace chromeos {
namespace cros_healthd {
namespace internal {
namespace {

class DataCollectorDelegateImpl : public DataCollector::Delegate {
 public:
  DataCollectorDelegateImpl();
  DataCollectorDelegateImpl(const DataCollectorDelegateImpl&) = delete;
  DataCollectorDelegateImpl& operator=(const DataCollectorDelegateImpl&) =
      delete;

 private:
  ~DataCollectorDelegateImpl() override;

  // DataCollector::Delegate override.
  std::string GetTouchpadLibraryName() override;
};

DataCollectorDelegateImpl::DataCollectorDelegateImpl() = default;

DataCollectorDelegateImpl::~DataCollectorDelegateImpl() = default;

std::string DataCollectorDelegateImpl::GetTouchpadLibraryName() {
#if defined(USE_LIBINPUT)
  base::FileEnumerator file_enum(base::FilePath("/dev/input/"), false,
                                 base::FileEnumerator::FileType::FILES);
  for (auto path = file_enum.Next(); !path.empty(); path = file_enum.Next()) {
    base::ScopedFD fd(
        HANDLE_EINTR(open(path.value().c_str(), O_RDWR | O_NONBLOCK)));
    if (fd.get() < 0) {
      LOG(ERROR) << "Couldn't open device path " << path;
      continue;
    }

    auto devinfo = std::make_unique<ui::EventDeviceInfo>();
    if (!devinfo->Initialize(fd.get(), path)) {
      LOG(ERROR) << "Failed to get device info for " << path;
      continue;
    }

    if (!devinfo->HasTouchpad() ||
        devinfo->device_type() != ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      continue;
    }

    if (devinfo->UseLibinput()) {
      return "libinput";
    }
  }
#endif

#if defined(USE_EVDEV_GESTURES)
  return "gestures";
#else
  return "Default EventConverterEvdev";
#endif
}

DataCollectorDelegateImpl* GetDataCollectorDelegate() {
  static base::NoDestructor<DataCollectorDelegateImpl> delegate;
  return delegate.get();
}

class DataCollectorImpl : public DataCollector,
                          public mojom::ChromiumDataCollector {
 public:
  explicit DataCollectorImpl(Delegate* delegate);
  DataCollectorImpl(const DataCollectorImpl&) = delete;
  DataCollectorImpl& operator=(const DataCollectorImpl&) = delete;
  ~DataCollectorImpl() override;

  // DataCollector overrides.
  void BindReceiver(
      mojo::PendingReceiver<mojom::ChromiumDataCollector> receiver) override;

 private:
  // mojom::ChromiumDataCollector overrides.
  void GetTouchscreenDevices(GetTouchscreenDevicesCallback callback) override;
  void GetTouchpadLibraryName(GetTouchpadLibraryNameCallback callback) override;

  // Pointer to the delegate.
  Delegate* const delegate_;
  // The receiver set to keep the mojo receivers.
  mojo::ReceiverSet<mojom::ChromiumDataCollector> receiver_set_;
};

DataCollectorImpl::DataCollectorImpl(Delegate* delegate)
    : delegate_(delegate) {}

DataCollectorImpl::~DataCollectorImpl() = default;

void DataCollectorImpl::BindReceiver(
    mojo::PendingReceiver<mojom::ChromiumDataCollector> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

mojom::InputDevice::ConnectionType GetInputDeviceConnectionType(
    ui::InputDeviceType type) {
  switch (type) {
    case ui::INPUT_DEVICE_INTERNAL:
      return mojom::InputDevice::ConnectionType::kInternal;
    case ui::INPUT_DEVICE_USB:
      return mojom::InputDevice::ConnectionType::kUSB;
    case ui::INPUT_DEVICE_BLUETOOTH:
      return mojom::InputDevice::ConnectionType::kBluetooth;
    case ui::INPUT_DEVICE_UNKNOWN:
      return mojom::InputDevice::ConnectionType::kUnknown;
  }
}

void GetTouchscreenDevicesOnUIThread(
    DataCollectorImpl::GetTouchscreenDevicesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const std::vector<ui::TouchscreenDevice>& devices =
      ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices();
  std::vector<mojom::TouchscreenDevicePtr> results;
  for (const auto& device : devices) {
    auto result = mojom::TouchscreenDevice::New();
    result->input_device = mojom::InputDevice::New();
    result->input_device->name = device.name;
    result->input_device->connection_type =
        GetInputDeviceConnectionType(device.type);
    result->input_device->physical_location = device.phys;
    result->input_device->is_enabled = device.enabled;
    result->input_device->sysfs_path = device.sys_path.value();

    result->touch_points = device.touch_points;
    result->has_stylus = device.has_stylus;
    result->has_stylus_garage_switch = device.has_stylus_garage_switch;
    results.push_back(std::move(result));
  }
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(results)));
}

void DataCollectorImpl::GetTouchscreenDevices(
    GetTouchscreenDevicesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetTouchscreenDevicesOnUIThread, std::move(callback)));
}

void DataCollectorImpl::GetTouchpadLibraryName(
    GetTouchpadLibraryNameCallback callback) {
  std::move(callback).Run(delegate_->GetTouchpadLibraryName());
}

// The pointer to the global instance.
DataCollector* g_instance = nullptr;

};  // namespace

DataCollector::DataCollector() {
  CHECK(!g_instance) << "Can have only one instance";
  g_instance = this;
}

DataCollector::~DataCollector() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
void DataCollector::Initialize() {
  new DataCollectorImpl(GetDataCollectorDelegate());
}

// static
void DataCollector::InitializeWithDelegateForTesting(Delegate* delegate) {
  new DataCollectorImpl(delegate);
}

// static
void DataCollector::Shutdown() {
  delete g_instance;
}

// static
DataCollector* DataCollector::Get() {
  CHECK(g_instance) << "Not initialized.";
  return g_instance;
}

}  // namespace internal
}  // namespace cros_healthd
}  // namespace chromeos
