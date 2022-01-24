// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/image_burner/image_burner_client.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// The ImageBurnerClient implementation.
class ImageBurnerClientImpl : public ImageBurnerClient {
 public:
  ImageBurnerClientImpl() : proxy_(nullptr) {}

  ImageBurnerClientImpl(const ImageBurnerClientImpl&) = delete;
  ImageBurnerClientImpl& operator=(const ImageBurnerClientImpl&) = delete;

  ~ImageBurnerClientImpl() override = default;

  // ImageBurnerClient override.
  void BurnImage(const std::string& from_path,
                 const std::string& to_path,
                 ErrorCallback error_callback) override {
    dbus::MethodCall method_call(imageburn::kImageBurnServiceInterface,
                                 imageburn::kBurnImage);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(from_path);
    writer.AppendString(to_path);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&ImageBurnerClientImpl::OnBurnImage,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      std::move(error_callback)));
  }

  // ImageBurnerClient override.
  void SetEventHandlers(
      BurnFinishedHandler burn_finished_handler,
      const BurnProgressUpdateHandler& burn_progress_update_handler) override {
    burn_finished_handler_ = std::move(burn_finished_handler);
    burn_progress_update_handler_ = burn_progress_update_handler;
  }

  // ImageBurnerClient override.
  void ResetEventHandlers() override {
    burn_finished_handler_.Reset();
    burn_progress_update_handler_.Reset();
  }

 protected:
  void Init(dbus::Bus* bus) override {
    proxy_ =
        bus->GetObjectProxy(imageburn::kImageBurnServiceName,
                            dbus::ObjectPath(imageburn::kImageBurnServicePath));
    proxy_->ConnectToSignal(
        imageburn::kImageBurnServiceInterface,
        imageburn::kSignalBurnFinishedName,
        base::BindRepeating(&ImageBurnerClientImpl::OnBurnFinished,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ImageBurnerClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    proxy_->ConnectToSignal(
        imageburn::kImageBurnServiceInterface, imageburn::kSignalBurnUpdateName,
        base::BindRepeating(&ImageBurnerClientImpl::OnBurnProgressUpdate,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ImageBurnerClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Called when a response for BurnImage is received
  void OnBurnImage(ErrorCallback error_callback, dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }
  }

  // Handles burn_finished signal and calls |handler|.
  void OnBurnFinished(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string target_path;
    bool success;
    std::string error;
    if (!reader.PopString(&target_path) || !reader.PopBool(&success) ||
        !reader.PopString(&error)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    if (burn_finished_handler_)
      std::move(burn_finished_handler_).Run(target_path, success, error);
  }

  // Handles burn_progress_udpate signal and calls |handler|.
  void OnBurnProgressUpdate(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string target_path;
    int64_t num_bytes_burnt;
    int64_t total_size;
    if (!reader.PopString(&target_path) || !reader.PopInt64(&num_bytes_burnt) ||
        !reader.PopInt64(&total_size)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    if (!burn_progress_update_handler_.is_null())
      burn_progress_update_handler_.Run(target_path, num_bytes_burnt,
                                        total_size);
  }

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded) {
    LOG_IF(ERROR, !succeeded)
        << "Connect to " << interface << " " << signal << " failed.";
  }

  dbus::ObjectProxy* proxy_;
  BurnFinishedHandler burn_finished_handler_;
  BurnProgressUpdateHandler burn_progress_update_handler_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ImageBurnerClientImpl> weak_ptr_factory_{this};
};

}  // namespace

ImageBurnerClient::ImageBurnerClient() = default;

ImageBurnerClient::~ImageBurnerClient() = default;

// static
std::unique_ptr<ImageBurnerClient> ImageBurnerClient::Create() {
  return std::make_unique<ImageBurnerClientImpl>();
}

}  // namespace chromeos
