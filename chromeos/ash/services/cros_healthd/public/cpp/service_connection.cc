// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"

#include <fcntl.h>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/ash/components/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/cros_system_api/mojo/service_constants.h"
#include "ui/events/ozone/evdev/event_device_info.h"

#if !defined(USE_REAL_DBUS_CLIENTS)
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#endif

namespace ash::cros_healthd {

namespace {

// Production implementation of ServiceConnection.
class ServiceConnectionImpl : public ServiceConnection {
 public:
  ServiceConnectionImpl();

  ServiceConnectionImpl(const ServiceConnectionImpl&) = delete;
  ServiceConnectionImpl& operator=(const ServiceConnectionImpl&) = delete;

 protected:
  ~ServiceConnectionImpl() override = default;

 private:
  // ServiceConnection overrides:
  mojom::CrosHealthdDiagnosticsService* GetDiagnosticsService() override;
  mojom::CrosHealthdProbeService* GetProbeService() override;
  mojom::CrosHealthdEventService* GetEventService() override;
  void BindDiagnosticsService(
      mojo::PendingReceiver<mojom::CrosHealthdDiagnosticsService> service)
      override;
  void BindProbeService(
      mojo::PendingReceiver<mojom::CrosHealthdProbeService> service) override;
  void SetBindNetworkHealthServiceCallback(
      BindNetworkHealthServiceCallback callback) override;
  void SetBindNetworkDiagnosticsRoutinesCallback(
      BindNetworkDiagnosticsRoutinesCallback callback) override;
  void SendChromiumDataCollector(
      mojo::PendingRemote<internal::mojom::ChromiumDataCollector> remote)
      override;
  std::string FetchTouchpadLibraryName() override;
  void FlushForTesting() override;

  // Uses |bind_network_health_callback_| if set to bind a remote to the
  // NetworkHealthService and send the PendingRemote to the CrosHealthdService.
  void BindAndSendNetworkHealthService();

  // Uses |bind_network_diagnostics_callback_| if set to bind a remote to the
  // NetworkDiagnosticsRoutines interface and send the PendingRemote to
  // cros_healthd.
  void BindAndSendNetworkDiagnosticsRoutines();

  // Binds the factory interface |cros_healthd_service_factory_| to an
  // implementation in the cros_healthd daemon, if it is not already bound. The
  // binding is accomplished via D-Bus bootstrap.
  void EnsureCrosHealthdServiceFactoryIsBound();

  // Uses |cros_healthd_service_factory_| to bind the diagnostics service remote
  // to an implementation in the cros_healethd daemon, if it is not already
  // bound.
  void BindCrosHealthdDiagnosticsServiceIfNeeded();

  // Uses |cros_healthd_service_factory_| to bind the event service remote to an
  // implementation in the cros_healethd daemon, if it is not already bound.
  void BindCrosHealthdEventServiceIfNeeded();

  // Uses |cros_healthd_service_factory_| to bind the probe service remote to an
  // implementation in the cros_healethd daemon, if it is not already bound.
  void BindCrosHealthdProbeServiceIfNeeded();

  // Mojo disconnect handler. Resets |cros_healthd_service_|, which will be
  // reconnected upon next use.
  void OnDisconnect();

  // Response callback for BootstrapMojoConnection.
  void OnBootstrapMojoConnectionResponse(bool success);

  mojo::Remote<mojom::CrosHealthdServiceFactory> cros_healthd_service_factory_;
  mojo::Remote<mojom::CrosHealthdProbeService> cros_healthd_probe_service_;
  mojo::Remote<mojom::CrosHealthdDiagnosticsService>
      cros_healthd_diagnostics_service_;
  mojo::Remote<mojom::CrosHealthdEventService> cros_healthd_event_service_;

  // Repeating callback that binds a mojo::PendingRemote to the
  // NetworkHealthService and returns it.
  BindNetworkHealthServiceCallback bind_network_health_callback_;

  // Repeating callback that binds a mojo::PendingRemote to the
  // NetworkDiagnosticsRoutines interface and returns it.
  BindNetworkDiagnosticsRoutinesCallback bind_network_diagnostics_callback_;

  const bool use_service_manager_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ServiceConnectionImpl> weak_factory_{this};
};

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

void ServiceConnectionImpl::BindDiagnosticsService(
    mojo::PendingReceiver<mojom::CrosHealthdDiagnosticsService> service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (use_service_manager_) {
    mojo_service_manager::GetServiceManagerProxy()->Request(
        chromeos::mojo_services::kCrosHealthdDiagnostics, absl::nullopt,
        std::move(service).PassPipe());
  } else {
    EnsureCrosHealthdServiceFactoryIsBound();
    cros_healthd_service_factory_->GetDiagnosticsService(std::move(service));
  }
}

void ServiceConnectionImpl::BindProbeService(
    mojo::PendingReceiver<mojom::CrosHealthdProbeService> service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (use_service_manager_) {
    mojo_service_manager::GetServiceManagerProxy()->Request(
        chromeos::mojo_services::kCrosHealthdProbe, absl::nullopt,
        std::move(service).PassPipe());
  } else {
    EnsureCrosHealthdServiceFactoryIsBound();
    cros_healthd_service_factory_->GetProbeService(std::move(service));
  }
}

void ServiceConnectionImpl::SetBindNetworkHealthServiceCallback(
    BindNetworkHealthServiceCallback callback) {
  // Don't set the interface if service manager is used.
  if (use_service_manager_)
    return;
  bind_network_health_callback_ = std::move(callback);
  BindAndSendNetworkHealthService();
}

void ServiceConnectionImpl::SetBindNetworkDiagnosticsRoutinesCallback(
    BindNetworkDiagnosticsRoutinesCallback callback) {
  // Don't set the interface if service manager is used.
  if (use_service_manager_)
    return;
  bind_network_diagnostics_callback_ = std::move(callback);
  BindAndSendNetworkDiagnosticsRoutines();
}

void ServiceConnectionImpl::SendChromiumDataCollector(
    mojo::PendingRemote<internal::mojom::ChromiumDataCollector> remote) {
  // Don't set the interface if service manager is used.
  if (use_service_manager_)
    return;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureCrosHealthdServiceFactoryIsBound();
  cros_healthd_service_factory_->SendChromiumDataCollector(std::move(remote));
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
  if (cros_healthd_service_factory_.is_bound())
    cros_healthd_service_factory_.FlushForTesting();
  if (cros_healthd_probe_service_.is_bound())
    cros_healthd_probe_service_.FlushForTesting();
  if (cros_healthd_diagnostics_service_.is_bound())
    cros_healthd_diagnostics_service_.FlushForTesting();
  if (cros_healthd_event_service_.is_bound())
    cros_healthd_event_service_.FlushForTesting();
}

void ServiceConnectionImpl::BindAndSendNetworkHealthService() {
  DCHECK(!use_service_manager_) << "ServiceFactory is not supported.";
  if (bind_network_health_callback_.is_null())
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureCrosHealthdServiceFactoryIsBound();
  auto remote = bind_network_health_callback_.Run();
  cros_healthd_service_factory_->SendNetworkHealthService(std::move(remote));
}

void ServiceConnectionImpl::BindAndSendNetworkDiagnosticsRoutines() {
  DCHECK(!use_service_manager_) << "ServiceFactory is not supported.";
  if (bind_network_diagnostics_callback_.is_null())
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureCrosHealthdServiceFactoryIsBound();
  auto remote = bind_network_diagnostics_callback_.Run();
  cros_healthd_service_factory_->SendNetworkDiagnosticsRoutines(
      std::move(remote));
}

void ServiceConnectionImpl::EnsureCrosHealthdServiceFactoryIsBound() {
  DCHECK(!use_service_manager_)
      << "ServiceFactory is not available in service manager.";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_service_factory_.is_bound())
    return;

  auto* client = CrosHealthdClient::Get();
  if (!client)
    return;

  cros_healthd_service_factory_ = client->BootstrapMojoConnection(
      base::BindOnce(&ServiceConnectionImpl::OnBootstrapMojoConnectionResponse,
                     weak_factory_.GetWeakPtr()));

  cros_healthd_service_factory_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, weak_factory_.GetWeakPtr()));
}

void ServiceConnectionImpl::BindCrosHealthdDiagnosticsServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_diagnostics_service_.is_bound())
    return;

  BindDiagnosticsService(
      cros_healthd_diagnostics_service_.BindNewPipeAndPassReceiver());
  cros_healthd_diagnostics_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, weak_factory_.GetWeakPtr()));
}

void ServiceConnectionImpl::BindCrosHealthdEventServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_event_service_.is_bound())
    return;

  if (use_service_manager_) {
    mojo_service_manager::GetServiceManagerProxy()->Request(
        chromeos::mojo_services::kCrosHealthdEvent, absl::nullopt,
        cros_healthd_event_service_.BindNewPipeAndPassReceiver().PassPipe());
  } else {
    EnsureCrosHealthdServiceFactoryIsBound();
    cros_healthd_service_factory_->GetEventService(
        cros_healthd_event_service_.BindNewPipeAndPassReceiver());
  }
  cros_healthd_event_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, weak_factory_.GetWeakPtr()));
}

void ServiceConnectionImpl::BindCrosHealthdProbeServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_probe_service_.is_bound())
    return;

  BindProbeService(cros_healthd_probe_service_.BindNewPipeAndPassReceiver());
  cros_healthd_probe_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, weak_factory_.GetWeakPtr()));
}

ServiceConnectionImpl::ServiceConnectionImpl()
    : use_service_manager_(mojo_service_manager::IsServiceManagerBound()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
#if !defined(USE_REAL_DBUS_CLIENTS)
  // Creates the fake mojo service if need. This is for browser test to do the
  // initialized.
  // TODO(b/230064284): Remove this after we migrate to mojo service manager.
  if (!FakeCrosHealthd::Get()) {
    CHECK(CrosHealthdClient::Get())
        << "The dbus client is not initialized. This should not happen in "
           "browser tests. In unit tests, use FakeCrosHealthd::Initialize() to "
           "initialize the fake cros healthd service.";
    // Only initialize the fake if fake dbus client is used.
    if (FakeCrosHealthdClient::Get())
      FakeCrosHealthd::Initialize();
  }
#endif  // defined(USE_REAL_DBUS_CLIENTS)
  if (!use_service_manager_)
    EnsureCrosHealthdServiceFactoryIsBound();
}

void ServiceConnectionImpl::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Connection errors are not expected, so log a warning.
  DLOG(WARNING) << "cros_healthd Mojo connection closed.";
  cros_healthd_service_factory_.reset();
  cros_healthd_probe_service_.reset();
  cros_healthd_diagnostics_service_.reset();
  cros_healthd_event_service_.reset();

  // Don't try to reconnect if service manager is used.
  if (use_service_manager_)
    return;

  EnsureCrosHealthdServiceFactoryIsBound();
  // If the cros_healthd_service_factory_ was able to be rebound, resend the
  // Chrome services to the CrosHealthd instance.
  if (cros_healthd_service_factory_.is_bound()) {
    BindAndSendNetworkHealthService();
    BindAndSendNetworkDiagnosticsRoutines();
  }
}

void ServiceConnectionImpl::OnBootstrapMojoConnectionResponse(
    const bool success) {
  DCHECK(!use_service_manager_)
      << "D-Bus is not used if service manager is used.";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    DLOG(WARNING) << "BootstrapMojoConnection D-Bus call failed.";
    cros_healthd_service_factory_.reset();
  }
}

}  // namespace

ServiceConnection* ServiceConnection::GetInstance() {
  static base::NoDestructor<ServiceConnectionImpl> service_connection;
  return service_connection.get();
}

}  // namespace ash::cros_healthd
