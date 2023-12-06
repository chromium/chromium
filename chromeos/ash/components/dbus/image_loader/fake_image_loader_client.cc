// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/image_loader/fake_image_loader_client.h"

#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

FakeImageLoaderClient::FakeImageLoaderClient() = default;

FakeImageLoaderClient::~FakeImageLoaderClient() = default;

void FakeImageLoaderClient::SetMountPathForComponent(
    const std::string& component_name,
    const base::FilePath& mount_path) {
  mount_paths_[component_name] = mount_path;
}

bool FakeImageLoaderClient::IsLoaded(const std::string& name) const {
  return base::Contains(loaded_components_, name);
}

base::FilePath FakeImageLoaderClient::GetComponentInstallPath(
    const std::string& name) const {
  if (!IsLoaded(name))
    return base::FilePath();

  const auto it = component_install_paths_.find(name);
  if (it == component_install_paths_.end())
    return base::FilePath();
  return it->second;
}

void FakeImageLoaderClient::RegisterComponent(
    const std::string& name,
    const std::string& version,
    const std::string& component_folder_abs_path,
    chromeos::DBusMethodCallback<bool> callback) {
  registered_components_[name] = version;
  component_install_paths_[name] =
      base::FilePath(component_folder_abs_path).AppendASCII(version);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::make_optional(true)));
}

void FakeImageLoaderClient::LoadComponent(
    const std::string& name,
    chromeos::DBusMethodCallback<std::string> callback) {
  const auto& version_it = registered_components_.find(name);
  if (version_it == registered_components_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  const auto& mount_path_it = mount_paths_.find(name);
  if (mount_path_it == mount_paths_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  loaded_components_.insert(name);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          std::make_optional(
              mount_path_it->second.Append(version_it->second).value())));
}

void FakeImageLoaderClient::LoadComponentAtPath(
    const std::string& name,
    const base::FilePath& path,
    chromeos::DBusMethodCallback<base::FilePath> callback) {
  const auto& mount_path_it = mount_paths_.find(name);
  if (mount_path_it == mount_paths_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  loaded_components_.insert(name);
  component_install_paths_[name] = path;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::make_optional(mount_path_it->second)));
}

void FakeImageLoaderClient::RemoveComponent(
    const std::string& name,
    chromeos::DBusMethodCallback<bool> callback) {
  registered_components_.erase(name);
  component_install_paths_.erase(name);
  UnmountComponent(name, std::move(callback));
}

void FakeImageLoaderClient::RequestComponentVersion(
    const std::string& name,
    chromeos::DBusMethodCallback<std::string> callback) {
  const auto& version_it = registered_components_.find(name);
  if (version_it == registered_components_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::make_optional(version_it->second)));
}

void FakeImageLoaderClient::UnmountComponent(
    const std::string& name,
    chromeos::DBusMethodCallback<bool> callback) {
  loaded_components_.erase(name);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::make_optional(true)));
}

}  // namespace ash
