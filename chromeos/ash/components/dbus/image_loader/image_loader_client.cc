// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/image_loader/image_loader_client.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/image_loader/fake_image_loader_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

ImageLoaderClient* g_instance = nullptr;

class ImageLoaderClientImpl : public ImageLoaderClient {
 public:
  ImageLoaderClientImpl() = default;

  ImageLoaderClientImpl(const ImageLoaderClientImpl&) = delete;
  ImageLoaderClientImpl& operator=(const ImageLoaderClientImpl&) = delete;

  ~ImageLoaderClientImpl() override = default;

  void RegisterComponent(const std::string& name,
                         const std::string& version,
                         const std::string& component_folder_abs_path,
                         chromeos::DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(imageloader::kImageLoaderServiceInterface,
                                 imageloader::kRegisterComponent);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    writer.AppendString(version);
    writer.AppendString(component_folder_abs_path);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&ImageLoaderClientImpl::OnBoolMethod,
                                      std::move(callback)));
  }

  void LoadComponent(
      const std::string& name,
      chromeos::DBusMethodCallback<std::string> callback) override {
    dbus::MethodCall method_call(imageloader::kImageLoaderServiceInterface,
                                 imageloader::kLoadComponent);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&ImageLoaderClientImpl::OnStringMethod,
                                      std::move(callback)));
  }

  void LoadComponentAtPath(
      const std::string& name,
      const base::FilePath& path,
      chromeos::DBusMethodCallback<base::FilePath> callback) override {
    dbus::MethodCall method_call(imageloader::kImageLoaderServiceInterface,
                                 imageloader::kLoadComponentAtPath);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    writer.AppendString(path.value());
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&ImageLoaderClientImpl::OnFilePathMethod,
                                      std::move(callback)));
  }

  void RemoveComponent(const std::string& name,
                       chromeos::DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(imageloader::kImageLoaderServiceInterface,
                                 imageloader::kRemoveComponent);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&ImageLoaderClientImpl::OnBoolMethod,
                                      std::move(callback)));
  }

  void RequestComponentVersion(
      const std::string& name,
      chromeos::DBusMethodCallback<std::string> callback) override {
    dbus::MethodCall method_call(imageloader::kImageLoaderServiceInterface,
                                 imageloader::kGetComponentVersion);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&ImageLoaderClientImpl::OnStringMethod,
                                      std::move(callback)));
  }

  void UnmountComponent(const std::string& name,
                        chromeos::DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(imageloader::kImageLoaderServiceInterface,
                                 imageloader::kUnmountComponent);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&ImageLoaderClientImpl::OnBoolMethod,
                                      std::move(callback)));
  }

  // chromeos::DBusClient override.
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        imageloader::kImageLoaderServiceName,
        dbus::ObjectPath(imageloader::kImageLoaderServicePath));
  }

 private:
  static void OnBoolMethod(chromeos::DBusMethodCallback<bool> callback,
                           dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    bool result = false;
    if (!reader.PopBool(&result)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(result);
  }

  static void OnStringMethod(chromeos::DBusMethodCallback<std::string> callback,
                             dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    std::string result;
    if (!reader.PopString(&result)) {
      std::move(callback).Run(std::nullopt);
      LOG(ERROR) << "Invalid response: " << response->ToString();
      return;
    }
    std::move(callback).Run(std::move(result));
  }

  static void OnFilePathMethod(
      chromeos::DBusMethodCallback<base::FilePath> callback,
      dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    std::string result;
    if (!reader.PopString(&result)) {
      std::move(callback).Run(std::nullopt);
      LOG(ERROR) << "Invalid response: " << response->ToString();
      return;
    }
    std::move(callback).Run(base::FilePath(std::move(result)));
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;
};

}  // namespace

// static
ImageLoaderClient* ImageLoaderClient::Get() {
  return g_instance;
}

// static
void ImageLoaderClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new ImageLoaderClientImpl())->Init(bus);
}

// static
void ImageLoaderClient::InitializeFake() {
  (new FakeImageLoaderClient())->Init(nullptr);
}

// static
void ImageLoaderClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

ImageLoaderClient::ImageLoaderClient() {
  CHECK(!g_instance);
  g_instance = this;
}

ImageLoaderClient::~ImageLoaderClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
