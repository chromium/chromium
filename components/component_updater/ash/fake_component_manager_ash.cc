// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/ash/fake_component_manager_ash.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/version.h"

namespace component_updater {

FakeComponentManagerAsh::ComponentInfo::ComponentInfo(
    Error load_response,
    const base::FilePath& install_path,
    const base::FilePath& mount_path)
    : load_response(load_response),
      install_path(install_path),
      mount_path(mount_path) {
  // If component load fails, neither install nor mount path should be set.
  DCHECK(load_response == Error::NONE ||
         (install_path.empty() && mount_path.empty()));
  // Component should have install path set if it's expected to be loaded.
  DCHECK(load_response != Error::NONE || (!install_path.empty()));
}

FakeComponentManagerAsh::ComponentInfo::ComponentInfo(
    Error load_response,
    const base::FilePath& install_path,
    const base::FilePath& mount_path,
    const base::Version& version)
    : load_response(load_response),
      install_path(install_path),
      mount_path(mount_path),
      version(version) {
  // If component load fails, neither install nor mount path should be set and
  // version should be invalid.
  DCHECK(load_response == Error::NONE ||
         (install_path.empty() && mount_path.empty() && !version.IsValid()));
  // Component should have install path and version set if it's expected to be
  // loaded.
  DCHECK(load_response != Error::NONE ||
         (!install_path.empty() && version.IsValid()));
}

FakeComponentManagerAsh::ComponentInfo::ComponentInfo(
    const FakeComponentManagerAsh::ComponentInfo& other) = default;

FakeComponentManagerAsh::ComponentInfo&
FakeComponentManagerAsh::ComponentInfo::operator=(
    const FakeComponentManagerAsh::ComponentInfo& other) = default;

FakeComponentManagerAsh::ComponentInfo::~ComponentInfo() = default;

FakeComponentManagerAsh::FakeComponentManagerAsh() = default;

FakeComponentManagerAsh::~FakeComponentManagerAsh() = default;

bool FakeComponentManagerAsh::FinishLoadRequest(
    const std::string& name,
    const ComponentInfo& component_info) {
  if (!pending_loads_.count(name) || pending_loads_[name].empty()) {
    LOG(ERROR) << "No pending load for " << name;
    return false;
  }

  auto& pending_load = pending_loads_[name].front();
  FinishComponentLoad(name, pending_load.mount_requested, component_info);

  LoadCallback callback = std::move(pending_load.callback);
  pending_loads_[name].pop_front();

  std::move(callback).Run(component_info.load_response,
                          component_info.load_response == Error::NONE
                              ? component_info.mount_path
                              : base::FilePath());
  return true;
}

bool FakeComponentManagerAsh::ResetComponentState(const std::string& name,
                                                   const ComponentInfo& state) {
  if (!supported_components_.count(name)) {
    return false;
  }

  installed_components_.erase(name);
  mounted_components_.erase(name);

  component_infos_.erase(name);
  component_infos_.emplace(name, ComponentInfo(state));
  return true;
}

bool FakeComponentManagerAsh::HasPendingInstall(
    const std::string& name) const {
  DCHECK(queue_load_requests_);

  const auto& it = pending_loads_.find(name);
  return it != pending_loads_.end() && !it->second.empty();
}

bool FakeComponentManagerAsh::UpdateRequested(const std::string& name) const {
  DCHECK(queue_load_requests_);

  const auto& it = pending_loads_.find(name);
  return it != pending_loads_.end() && !it->second.empty() &&
         it->second.front().needs_update;
}

void FakeComponentManagerAsh::SetDelegate(Delegate* delegate) {
  // No-op, not used by the fake.
}

void FakeComponentManagerAsh::Load(const std::string& name,
                                    MountPolicy mount_policy,
                                    UpdatePolicy update_policy,
                                    LoadCallback load_callback) {
  if (!supported_components_.count(name)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(load_callback),
                                  Error::UNKNOWN_COMPONENT, base::FilePath()));
    return;
  }

  bool needs_update = update_policy == UpdatePolicy::kForce ||
                      (!installed_components_.count(name) &&
                       update_policy != UpdatePolicy::kSkip);

  // The request has to be handled if the component is not yet installed, or it
  // requires immediate update.
  if (needs_update || !installed_components_.count(name)) {
    HandlePendingRequest(name, mount_policy == MountPolicy::kMount,
                         needs_update, std::move(load_callback));
    return;
  }

  // Handle request if the component has yet to be mounted - e.g. if previous
  // loads installed the component without mounting it.
  if (!mounted_components_.count(name) && mount_policy == MountPolicy::kMount) {
    HandlePendingRequest(name, true /*mount_requested*/, false /*needs_update*/,
                         std::move(load_callback));
    return;
  }

  // The component has been prevoiusly installed, and mounted as required by
  // this load request - run the callback according to the existing state.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(load_callback), Error::NONE,
                                mount_policy == MountPolicy::kMount
                                    ? mounted_components_[name]
                                    : base::FilePath()));
}

bool FakeComponentManagerAsh::Unload(const std::string& name) {
  {
    base::AutoLock lock(registered_components_lock_);
    registered_components_.erase(name);
  }
  mounted_components_.erase(name);
  installed_components_.erase(name);
  return unload_component_result_;
}

void FakeComponentManagerAsh::RegisterCompatiblePath(
    const std::string& name,
    CompatibleComponentInfo info) {
  installed_components_[name] = std::move(info);
}

void FakeComponentManagerAsh::UnregisterCompatiblePath(
    const std::string& name) {
  installed_components_.erase(name);
}

base::FilePath FakeComponentManagerAsh::GetCompatiblePath(
    const std::string& name) const {
  const auto& it = installed_components_.find(name);
  if (it == installed_components_.end()) {
    return base::FilePath();
  }
  return it->second.path;
}

void FakeComponentManagerAsh::SetRegisteredComponents(
    const std::set<std::string>& components) {
  base::AutoLock lock(registered_components_lock_);
  registered_components_ = components;
}

bool FakeComponentManagerAsh::IsRegisteredMayBlock(const std::string& name) {
  base::AutoLock lock(registered_components_lock_);
  return registered_components_.count(name);
}

void FakeComponentManagerAsh::RegisterInstalled() {
  NOTIMPLEMENTED();
}

void FakeComponentManagerAsh::GetVersion(
    const std::string& name,
    base::OnceCallback<void(const base::Version&)> version_callback) const {
  const auto& component_info = component_infos_.find(name);
  if (component_info == component_infos_.end() ||
      !component_info->second.version.has_value()) {
    std::move(version_callback).Run(base::Version());
    return;
  }

  std::move(version_callback).Run(component_info->second.version.value());
}

FakeComponentManagerAsh::LoadRequest::LoadRequest(bool mount_requested,
                                                   bool needs_update,
                                                   LoadCallback callback)
    : mount_requested(mount_requested),
      needs_update(needs_update),
      callback(std::move(callback)) {}

FakeComponentManagerAsh::LoadRequest::~LoadRequest() = default;

void FakeComponentManagerAsh::HandlePendingRequest(const std::string& name,
                                                    bool mount_requested,
                                                    bool needs_update,
                                                    LoadCallback callback) {
  if (queue_load_requests_) {
    pending_loads_[name].emplace_back(mount_requested, needs_update,
                                      std::move(callback));
    return;
  }

  const auto& component_info = component_infos_.find(name);
  if (component_info == component_infos_.end()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), Error::INSTALL_FAILURE,
                                  base::FilePath()));
    return;
  }

  FinishComponentLoad(name, mount_requested, component_info->second);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), component_info->second.load_response,
                     component_info->second.mount_path));
}

void FakeComponentManagerAsh::FinishComponentLoad(
    const std::string& name,
    bool mount_requested,
    const ComponentInfo& component_info) {
  {
    base::AutoLock lock(registered_components_lock_);
    registered_components_.insert(name);
  }

  if (component_info.load_response != Error::NONE) {
    return;
  }

  DCHECK_EQ(mount_requested, !component_info.mount_path.empty());
  installed_components_[name] = CompatibleComponentInfo(
      component_info.install_path, component_info.version);
  if (mount_requested) {
    mounted_components_[name] = component_info.mount_path;
  }
}

}  // namespace component_updater
