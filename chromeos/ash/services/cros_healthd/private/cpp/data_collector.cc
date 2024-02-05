// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/private/cpp/data_collector.h"

#include <fcntl.h>

#include "ash/display/privacy_screen_controller.h"
#include "ash/shell.h"
#include "base/check_op.h"
#include "base/files/file_enumerator.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/mojo/service_constants.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ash::cros_healthd::internal {

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
  bool IsPrivacyScreenSupported() override;
  bool IsPrivacyScreenManaged() override;
  void SetPrivacyScreenState(bool state) override;
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

bool DataCollectorDelegateImpl::IsPrivacyScreenSupported() {
  return Shell::Get()->privacy_screen_controller()->IsSupported();
}

bool DataCollectorDelegateImpl::IsPrivacyScreenManaged() {
  return Shell::Get()->privacy_screen_controller()->IsManaged();
}

void DataCollectorDelegateImpl::SetPrivacyScreenState(bool state) {
  Shell::Get()->privacy_screen_controller()->SetEnabled(state);
}

DataCollectorDelegateImpl* GetDataCollectorDelegate() {
  static base::NoDestructor<DataCollectorDelegateImpl> delegate;
  return delegate.get();
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
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    DataCollector::GetTouchscreenDevicesCallback callback) {
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
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(results)));
}

}  // namespace

DataCollector::DataCollector() : DataCollector(GetDataCollectorDelegate()) {
  if (mojo_service_manager::IsServiceManagerBound()) {
    mojo_service_manager::GetServiceManagerProxy()->Register(
        chromeos::mojo_services::kChromiumCrosHealthdDataCollector,
        provider_receiver_.BindNewPipeAndPassRemote());
  }
}

DataCollector::DataCollector(Delegate* delegate) : delegate_(delegate) {}

DataCollector::~DataCollector() = default;

mojo::PendingRemote<mojom::ChromiumDataCollector>
DataCollector::BindNewPipeAndPassRemote() {
  mojo::PendingRemote<mojom::ChromiumDataCollector> remote;
  receiver_set_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void DataCollector::GetTouchscreenDevices(
    GetTouchscreenDevicesCallback callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&GetTouchscreenDevicesOnUIThread,
                                base::SequencedTaskRunner::GetCurrentDefault(),
                                std::move(callback)));
}

void DataCollector::GetTouchpadLibraryName(
    GetTouchpadLibraryNameCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&Delegate::GetTouchpadLibraryName,
                     base::Unretained(delegate_)),
      std::move(callback));
}

void DataCollector::SetPrivacyScreenState(
    bool state,
    SetPrivacyScreenStateCallback callback) {
  if (!delegate_->IsPrivacyScreenSupported() ||
      delegate_->IsPrivacyScreenManaged()) {
    std::move(callback).Run(false);
    return;
  }

  delegate_->SetPrivacyScreenState(state);
  std::move(callback).Run(true);
}

void DataCollector::DEPRECATED_SetAudioOutputMute(
    bool mute_on,
    DEPRECATED_SetAudioOutputMuteCallback callback) {
  std::move(callback).Run(/*success*/ false);
}

void DataCollector::Request(
    chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
    mojo::ScopedMessagePipeHandle receiver) {
  receiver_set_.Add(this, mojo::PendingReceiver<mojom::ChromiumDataCollector>(
                              std::move(receiver)));
}

}  // namespace ash::cros_healthd::internal
