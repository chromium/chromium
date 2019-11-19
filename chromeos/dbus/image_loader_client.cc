// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/image_loader_client.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class ImageLoaderClientImpl : public ImageLoaderClient {
 public:
  ImageLoaderClientImpl() = default;

  ~ImageLoaderClientImpl() override = default;

  void RegisterComponent(const std::string& name,
                         const std::string& version,
                         const std::string& component_folder_abs_path,
                         DBusMethodCallback<bool> callback) override {
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

  void LoadComponent(const std::string& name,
                     DBusMethodCallback<std::string> callback) override {
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
      DBusMethodCallback<base::FilePath> callback) override {
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
                       DBusMethodCallback<bool> callback) override {
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
      DBusMethodCallback<std::string> callback) override {
    dbus::MethodCall method_call(imageloader::kImageLoaderServiceInterface,
                                 imageloader::kGetComponentVersion);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&ImageLoaderClientImpl::OnStringMethod,
                                      std::move(callback)));
  }

  void UnmountComponent(const std::string& name,
                        DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(imageloader::kImageLoaderServiceInterface,
                                 imageloader::kUnmountComponent);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&ImageLoaderClientImpl::OnBoolMethod,
                                      std::move(callback)));
  }

 protected:
  // DBusClient override.
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        imageloader::kImageLoaderServiceName,
        dbus::ObjectPath(imageloader::kImageLoaderServicePath));
  }

 private:
  static void OnBoolMethod(DBusMethodCallback<bool> callback,
                           dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    bool result = false;
    if (!reader.PopBool(&result)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(result);
  }

  static void OnStringMethod(DBusMethodCallback<std::string> callback,
                             dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    std::string result;
    if (!reader.PopString(&result)) {
      std::move(callback).Run(base::nullopt);
      LOG(ERROR) << "Invalid response: " << response->ToString();
      return;
    }
    std::move(callback).Run(std::move(result));
  }

  static void OnFilePathMethod(DBusMethodCallback<base::FilePath> callback,
                               dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    std::string result;
    if (!reader.PopString(&result)) {
      std::move(callback).Run(base::nullopt);
      LOG(ERROR) << "Invalid response: " << response->ToString();
      return;
    }
    std::move(callback).Run(base::FilePath(std::move(result)));
  }

  dbus::ObjectProxy* proxy_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ImageLoaderClientImpl);
};

}  // namespace

ImageLoaderClient::ImageLoaderClient() = default;

ImageLoaderClient::~ImageLoaderClient() = default;

// static
std::unique_ptr<ImageLoaderClient> ImageLoaderClient::Create() {
  return std::make_unique<ImageLoaderClientImpl>();
}

}  // namespace chromeos
