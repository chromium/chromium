// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/media_analytics/media_analytics_client.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/media_analytics/fake_media_analytics_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

MediaAnalyticsClient* g_instance = nullptr;

}  // namespace

// The MediaAnalyticsClient implementation used in production.
class MediaAnalyticsClientImpl : public MediaAnalyticsClient {
 public:
  MediaAnalyticsClientImpl() = default;

  MediaAnalyticsClientImpl(const MediaAnalyticsClientImpl&) = delete;
  MediaAnalyticsClientImpl& operator=(const MediaAnalyticsClientImpl&) = delete;

  ~MediaAnalyticsClientImpl() override = default;

  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void GetState(chromeos::DBusMethodCallback<mri::State> callback) override {
    dbus::MethodCall method_call(media_perception::kMediaPerceptionServiceName,
                                 media_perception::kStateFunction);
    dbus_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaAnalyticsClientImpl::OnState,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetState(const mri::State& state,
                chromeos::DBusMethodCallback<mri::State> callback) override {
    DCHECK(state.has_status()) << "Attempting to SetState without status set.";

    dbus::MethodCall method_call(media_perception::kMediaPerceptionServiceName,
                                 media_perception::kStateFunction);

    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(state);

    dbus_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaAnalyticsClientImpl::OnState,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetDiagnostics(
      chromeos::DBusMethodCallback<mri::Diagnostics> callback) override {
    dbus::MethodCall method_call(media_perception::kMediaPerceptionServiceName,
                                 media_perception::kGetDiagnosticsFunction);
    // TODO(lasoren): Verify that this timeout setting is sufficient.
    dbus_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaAnalyticsClientImpl::OnGetDiagnostics,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void BootstrapMojoConnection(
      base::ScopedFD file_descriptor,
      chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(media_perception::kMediaPerceptionServiceName,
                                 media_perception::kBootstrapMojoConnection);
    dbus::MessageWriter writer(&method_call);

    writer.AppendFileDescriptor(file_descriptor.get());
    dbus_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &MediaAnalyticsClientImpl::OnBootstrapMojoConnectionCallback,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Init(dbus::Bus* bus) {
    dbus_proxy_ = bus->GetObjectProxy(
        media_perception::kMediaPerceptionServiceName,
        dbus::ObjectPath(media_perception::kMediaPerceptionServicePath));
    // Connect to the MediaPerception signal.
    dbus_proxy_->ConnectToSignal(
        media_perception::kMediaPerceptionInterface,
        media_perception::kDetectionSignal,
        base::BindRepeating(
            &MediaAnalyticsClientImpl::OnDetectionSignalReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&MediaAnalyticsClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnBootstrapMojoConnectionCallback(
      chromeos::VoidDBusMethodCallback callback,
      dbus::Response* response) {
    std::move(callback).Run(response != nullptr);
  }

  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded) {
    LOG_IF(ERROR, !succeeded)
        << "Connect to " << interface << " " << signal << " failed.";
  }

  // Handler that is triggered when a MediaPerception proto is received from
  // the media analytics process.
  void OnDetectionSignalReceived(dbus::Signal* signal) {
    mri::MediaPerception media_perception;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&media_perception)) {
      LOG(ERROR) << "Invalid detection signal: " << signal->ToString();
      return;
    }

    for (auto& observer : observer_list_)
      observer.OnDetectionSignal(media_perception);
  }

  void OnState(chromeos::DBusMethodCallback<mri::State> callback,
               dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Call to State failed to get response.";
      std::move(callback).Run(std::nullopt);
      return;
    }

    mri::State state;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&state)) {
      LOG(ERROR) << "Invalid D-Bus response: " << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(std::move(state));
  }

  void OnGetDiagnostics(chromeos::DBusMethodCallback<mri::Diagnostics> callback,
                        dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Call to GetDiagnostics failed to get response.";
      std::move(callback).Run(std::nullopt);
      return;
    }

    mri::Diagnostics diagnostics;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&diagnostics)) {
      LOG(ERROR) << "Invalid GetDiagnostics response: " << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(std::move(diagnostics));
  }

  raw_ptr<dbus::ObjectProxy> dbus_proxy_ = nullptr;
  base::ObserverList<Observer>::Unchecked observer_list_;
  base::WeakPtrFactory<MediaAnalyticsClientImpl> weak_ptr_factory_{this};
};

MediaAnalyticsClient::MediaAnalyticsClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

MediaAnalyticsClient::~MediaAnalyticsClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void MediaAnalyticsClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new MediaAnalyticsClientImpl())->Init(bus);
}

// static
void MediaAnalyticsClient::InitializeFake() {
  new FakeMediaAnalyticsClient();
}

// static
void MediaAnalyticsClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
MediaAnalyticsClient* MediaAnalyticsClient::Get() {
  return g_instance;
}

}  // namespace ash
