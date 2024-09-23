// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_ASH_FAKE_COMPONENT_MANAGER_ASH_H_
#define COMPONENTS_COMPONENT_UPDATER_ASH_FAKE_COMPONENT_MANAGER_ASH_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "base/version.h"
#include "components/component_updater/ash/component_manager_ash.h"

namespace component_updater {

// This fake implementation of cros component manager. Intended to be used in
// tests to abstract away the cros component manager dependency on imageloader
// and component updater services, and local file system.
class FakeComponentManagerAsh : public ComponentManagerAsh {
 public:
  // Information about how fake component manager should "load" a component.
  struct ComponentInfo {
    ComponentInfo(Error load_response,
                  const base::FilePath& install_path,
                  const base::FilePath& mount_path);
    ComponentInfo(Error load_response,
                  const base::FilePath& install_path,
                  const base::FilePath& mount_path,
                  const base::Version& version);
    ComponentInfo(const ComponentInfo& other);
    ComponentInfo& operator=(const ComponentInfo& other);
    ~ComponentInfo();

    // The status load requests for the component should produce.
    Error load_response;

    // The local path where the fake component manager thinks the component is
    // installed.
    base::FilePath install_path;

    // The path where the fake component manager thinks the component is
    // mounted.
    base::FilePath mount_path;

    // The version of the component. Must be set to use GetVersion().
    std::optional<base::Version> version;
  };

  FakeComponentManagerAsh();

  FakeComponentManagerAsh(const FakeComponentManagerAsh&) = delete;
  FakeComponentManagerAsh& operator=(const FakeComponentManagerAsh&) = delete;

  void set_queue_load_requests(bool queue_load_requests) {
    queue_load_requests_ = queue_load_requests;
  }
  void set_supported_components(const std::set<std::string>& components) {
    supported_components_ = components;
  }
  void set_unload_component_result(bool result) {
    unload_component_result_ = result;
  }
  void SetRegisteredComponents(const std::set<std::string>& components);

  // Finishes a queued component load request. Should be used only if
  // |queue_load_requests_| is set.
  bool FinishLoadRequest(const std::string& name, const ComponentInfo& state);

  // If the component is "loaded", clears the recorded install and mount paths,
  // and sets the info about how future load requests for the component should
  // be handled.
  bool ResetComponentState(const std::string& name, const ComponentInfo& state);

  // Whether any component loads are pending. Expected to be used only if
  // |queue_load_requests_| is set.
  bool HasPendingInstall(const std::string& name) const;

  // Whether the next pending component load requests triggers immediate
  // component update request. Expected to be used only if
  // |queue_load_requests_| is set.
  bool UpdateRequested(const std::string& name) const;

  // ComponentManagerAsh:
  void SetDelegate(Delegate* delegate) override;
  void Load(const std::string& name,
            MountPolicy mount_policy,
            UpdatePolicy update_policy,
            LoadCallback load_callback) override;
  bool Unload(const std::string& name) override;
  void GetVersion(const std::string& name,
                  base::OnceCallback<void(const base::Version&)>
                      version_callback) const override;
  void RegisterCompatiblePath(const std::string& name,
                              CompatibleComponentInfo info) override;
  void UnregisterCompatiblePath(const std::string& name) override;
  base::FilePath GetCompatiblePath(const std::string& name) const override;
  bool IsRegisteredMayBlock(const std::string& name) override;
  void RegisterInstalled() override;

 protected:
  ~FakeComponentManagerAsh() override;

 private:
  // Describes pending component load request.
  struct LoadRequest {
    LoadRequest(bool mount_requested, bool needs_update, LoadCallback callback);
    ~LoadRequest();

    // Whether the component should be mounted as part of the load request.
    bool mount_requested;

    // Whether the request should start immediate component update check.
    bool needs_update;

    // The load request callback.
    LoadCallback callback;
  };

  // Handles a load request for a component, either by queueing it (if
  // queue_load_requests_ is set), or setting the new component state depending
  // on component_infos_.
  // |name|: the component name.
  // |mount_requested|: whether the component mount was requested as part of the
  //     load request.
  // |needs_update|: whether the load request triggers immediate update attempt.
  // |callback|: to be called when the load request finishes.
  void HandlePendingRequest(const std::string& name,
                            bool mount_requested,
                            bool needs_update,
                            LoadCallback callback);
  // Updates the fake component loader state on a successful component load
  // request.
  // |name|: the component name.
  // |mount_requested|: whether the component should be mounted.
  // |component_info|: the component's load information.
  void FinishComponentLoad(const std::string& name,
                           bool mount_requested,
                           const ComponentInfo& component_info);

  // Whether the load requests should be queued up, and not handled immediately.
  // When this is set, component load requests should be completed using
  // FinishLoadRequest().
  bool queue_load_requests_ = false;

  bool unload_component_result_ = true;

  // Set of components that can be handled by this component manager.
  std::set<std::string> supported_components_;

  // Set of components registered with this component manager - used primarily
  // by IsRegistered() implementation.
  std::set<std::string> registered_components_;
  base::Lock registered_components_lock_;

  // The component information registered using ResetComponentInfo() - used to
  // handle component load requests when queue_load_requests_ is not set.
  std::map<std::string, ComponentInfo> component_infos_;

  // List of pending component load requests per component. Used only if
  // queue_load_requests_ is set.
  std::map<std::string, std::list<LoadRequest>> pending_loads_;

  // Maps the currently installed (and loaded) components to their installation
  // path.
  std::map<std::string, CompatibleComponentInfo> installed_components_;

  // Maps the currently mounted components to their mount point path.
  std::map<std::string, base::FilePath> mounted_components_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_ASH_FAKE_COMPONENT_MANAGER_ASH_H_
