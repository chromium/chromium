// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"

#include <fcntl.h>

#include <optional>

#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/cros_system_api/mojo/service_constants.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ash::cros_healthd {

namespace {

// Production implementation of ServiceConnection.
class ServiceConnectionImpl : public ServiceConnection {
 public:
  ServiceConnectionImpl();
  ServiceConnectionImpl(const ServiceConnectionImpl&) = delete;
  ServiceConnectionImpl& operator=(const ServiceConnectionImpl&) = delete;

 protected:
  ~ServiceConnectionImpl() override;

 private:
  // ServiceConnection overrides:
  mojom::CrosHealthdDiagnosticsService* GetDiagnosticsService() override;
  mojom::CrosHealthdProbeService* GetProbeService() override;
  mojom::CrosHealthdEventService* GetEventService() override;
  mojom::CrosHealthdRoutinesService* GetRoutinesService() override;
  void BindDiagnosticsService(
      mojo::PendingReceiver<mojom::CrosHealthdDiagnosticsService> service)
      override;
  void BindProbeService(
      mojo::PendingReceiver<mojom::CrosHealthdProbeService> service) override;
  std::string FetchTouchpadLibraryName() override;
  void FlushForTesting() override;

  // Binds the diagnostics service remote if it is not already bound.
  void BindCrosHealthdDiagnosticsServiceIfNeeded();

  // Binds the event service remote if it is not already bound.
  void BindCrosHealthdEventServiceIfNeeded();

  // Binds the routines service remote if it is not already bound.
  void BindCrosHealthdRoutinesServiceIfNeeded();

  // Binds the probe service remote if it is not already bound.
  void BindCrosHealthdProbeServiceIfNeeded();

  mojo::Remote<mojom::CrosHealthdProbeService> cros_healthd_probe_service_;
  mojo::Remote<mojom::CrosHealthdDiagnosticsService>
      cros_healthd_diagnostics_service_;
  mojo::Remote<mojom::CrosHealthdRoutinesService>
      cros_healthd_routines_service_;
  mojo::Remote<mojom::CrosHealthdEventService> cros_healthd_event_service_;

  SEQUENCE_CHECKER(sequence_checker_);
};

ServiceConnectionImpl::ServiceConnectionImpl() = default;

ServiceConnectionImpl::~ServiceConnectionImpl() = default;

mojom::CrosHealthdDiagnosticsService*
ServiceConnectionImpl::GetDiagnosticsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  return cros_healthd_diagnostics_service_.get();
}

mojom::CrosHealthdProbeService* ServiceConnectionImpl::GetProbeService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdProbeServiceIfNeeded();
  return cros_healthd_probe_service_.get();
}

mojom::CrosHealthdEventService* ServiceConnectionImpl::GetEventService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdEventServiceIfNeeded();
  return cros_healthd_event_service_.get();
}

mojom::CrosHealthdRoutinesService* ServiceConnectionImpl::GetRoutinesService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdRoutinesServiceIfNeeded();
  return cros_healthd_routines_service_.get();
}

void ServiceConnectionImpl::BindDiagnosticsService(
    mojo::PendingReceiver<mojom::CrosHealthdDiagnosticsService> service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosHealthdDiagnostics, std::nullopt,
      std::move(service).PassPipe());
}

void ServiceConnectionImpl::BindProbeService(
    mojo::PendingReceiver<mojom::CrosHealthdProbeService> service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosHealthdProbe, std::nullopt,
      std::move(service).PassPipe());
}

// This is a short-term solution for ChromeOS Flex. We should remove this work
// around after cros_healthd team develop a healthier input telemetry approach.
std::string ServiceConnectionImpl::FetchTouchpadLibraryName() {
#if defined(USE_LIBINPUT)
  base::FileEnumerator file_enum(base::FilePath("/dev/input/"), false,
                                 base::FileEnumerator::FileType::FILES);
  for (auto path = file_enum.Next(); !path.empty(); path = file_enum.Next()) {
    base::ScopedFD fd(open(path.value().c_str(), O_RDWR | O_NONBLOCK));
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

void ServiceConnectionImpl::FlushForTesting() {
  if (cros_healthd_probe_service_.is_bound()) {
    cros_healthd_probe_service_.FlushForTesting();
  }
  if (cros_healthd_diagnostics_service_.is_bound()) {
    cros_healthd_diagnostics_service_.FlushForTesting();
  }
  if (cros_healthd_event_service_.is_bound()) {
    cros_healthd_event_service_.FlushForTesting();
  }
  if (cros_healthd_routines_service_.is_bound()) {
    cros_healthd_routines_service_.FlushForTesting();
  }
}

void ServiceConnectionImpl::BindCrosHealthdDiagnosticsServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_diagnostics_service_.is_bound()) {
    return;
  }

  BindDiagnosticsService(
      cros_healthd_diagnostics_service_.BindNewPipeAndPassReceiver());
  cros_healthd_diagnostics_service_.reset_on_disconnect();
}

void ServiceConnectionImpl::BindCrosHealthdEventServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_event_service_.is_bound()) {
    return;
  }

  mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosHealthdEvent, std::nullopt,
      cros_healthd_event_service_.BindNewPipeAndPassReceiver().PassPipe());
  cros_healthd_event_service_.reset_on_disconnect();
}

void ServiceConnectionImpl::BindCrosHealthdRoutinesServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_routines_service_.is_bound()) {
    return;
  }

  mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosHealthdRoutines, std::nullopt,
      cros_healthd_routines_service_.BindNewPipeAndPassReceiver().PassPipe());
  cros_healthd_routines_service_.reset_on_disconnect();
}

void ServiceConnectionImpl::BindCrosHealthdProbeServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_probe_service_.is_bound()) {
    return;
  }

  BindProbeService(cros_healthd_probe_service_.BindNewPipeAndPassReceiver());
  cros_healthd_probe_service_.reset_on_disconnect();
}

}  // namespace

ServiceConnection* ServiceConnection::GetInstance() {
  static base::NoDestructor<ServiceConnectionImpl> service_connection;
  return service_connection.get();
}

}  // namespace ash::cros_healthd
