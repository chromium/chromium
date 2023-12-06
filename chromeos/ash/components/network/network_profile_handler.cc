// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_profile_handler.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/network/network_profile_observer.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

bool ConvertListValueToStringVector(const base::Value::List& string_list,
                                    std::vector<std::string>* result) {
  for (const base::Value& i : string_list) {
    const std::string* str = i.GetIfString();
    if (!str)
      return false;
    result->push_back(*str);
  }
  return true;
}

void LogProfileRequestError(const std::string& profile_path,
                            const std::string& error_name,
                            const std::string& error_message) {
  LOG(ERROR) << "Error when requesting properties for profile "
             << profile_path << ": " << error_message;
}

void LogError(const std::string& name,
              const std::string& profile_path,
              const std::string& dbus_error_name,
              const std::string& dbus_error_message) {
  LOG(ERROR) << name << " failed:"
             << " profile=" << profile_path
             << " dbus-error-name=" << dbus_error_name
             << " dbus-error-msg=" << dbus_error_message;
}

}  // namespace

// static
std::string NetworkProfileHandler::GetSharedProfilePath() {
  return ShillProfileClient::GetSharedProfilePath();
}

void NetworkProfileHandler::AddObserver(NetworkProfileObserver* observer) {
  observers_.AddObserver(observer);
}

void NetworkProfileHandler::RemoveObserver(NetworkProfileObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool NetworkProfileHandler::HasObserver(NetworkProfileObserver* observer) {
  return observers_.HasObserver(observer);
}

void NetworkProfileHandler::GetManagerPropertiesCallback(
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    LOG(ERROR) << "Error when requesting manager properties.";
    return;
  }

  const base::Value* profiles = properties->Find(shill::kProfilesProperty);
  if (!profiles) {
    LOG(ERROR) << "Manager properties returned from Shill don't contain "
               << "the field " << shill::kProfilesProperty;
    return;
  }
  OnPropertyChanged(shill::kProfilesProperty, *profiles);
}

void NetworkProfileHandler::OnPropertyChanged(const std::string& name,
                                              const base::Value& value) {
  if (name != shill::kProfilesProperty)
    return;

  DCHECK(value.is_list());

  std::vector<std::string> new_profile_paths;
  bool result =
      ConvertListValueToStringVector(value.GetList(), &new_profile_paths);
  DCHECK(result);

  VLOG(2) << "Profiles: " << profiles_.size();
  // Search for removed profiles.
  std::vector<std::string> removed_profile_paths;
  for (ProfileList::const_iterator it = profiles_.begin();
       it != profiles_.end(); ++it) {
    if (!base::Contains(new_profile_paths, it->path)) {
      removed_profile_paths.push_back(it->path);
    }
  }

  for (const std::string& profile_path : removed_profile_paths) {
    RemoveProfile(profile_path);
    // Also stop pending creations of this profile.
    pending_profile_creations_.erase(profile_path);
  }

  for (std::vector<std::string>::const_iterator it = new_profile_paths.begin();
       it != new_profile_paths.end(); ++it) {
    // Skip known profiles. The associated userhash should never change.
    if (GetProfileForPath(*it) || pending_profile_creations_.count(*it) > 0)
      continue;
    pending_profile_creations_.insert(*it);

    VLOG(2) << "Requesting properties of profile path " << *it << ".";
    ShillProfileClient::Get()->GetProperties(
        dbus::ObjectPath(*it),
        base::BindOnce(&NetworkProfileHandler::GetProfilePropertiesCallback,
                       weak_ptr_factory_.GetWeakPtr(), *it),
        base::BindOnce(&LogProfileRequestError, *it));
  }
}

void NetworkProfileHandler::GetProfilePropertiesCallback(
    const std::string& profile_path,
    base::Value::Dict properties) {
  if (pending_profile_creations_.erase(profile_path) == 0) {
    VLOG(1) << "Ignore received properties, profile was removed.";
    return;
  }
  if (GetProfileForPath(profile_path)) {
    VLOG(1) << "Ignore received properties, profile is already created.";
    return;
  }
  const std::string* userhash = properties.FindString(shill::kUserHashProperty);

  AddProfile(NetworkProfile(profile_path, userhash ? *userhash : ""));
}

void NetworkProfileHandler::AddProfile(const NetworkProfile& profile) {
  VLOG(2) << "Adding profile " << profile.ToDebugString() << ".";
  profiles_.push_back(profile);
  for (auto& observer : observers_) {
    observer.OnProfileAdded(profiles_.back());
  }
}

void NetworkProfileHandler::RemoveProfile(const std::string& profile_path) {
  VLOG(2) << "Removing profile for path " << profile_path << ".";
  ProfileList::iterator found =
      base::ranges::find(profiles_, profile_path, &NetworkProfile::path);
  if (found == profiles_.end()) {
    return;
  }
  NetworkProfile profile = *found;
  profiles_.erase(found);
  for (auto& observer : observers_) {
    observer.OnProfileRemoved(profile);
  }
}

const NetworkProfile* NetworkProfileHandler::GetProfileForPath(
    const std::string& profile_path) const {
  ProfileList::const_iterator found =
      base::ranges::find(profiles_, profile_path, &NetworkProfile::path);

  if (found == profiles_.end()) {
    return nullptr;
  }
  return &*found;
}

const NetworkProfile* NetworkProfileHandler::GetProfileForUserhash(
    const std::string& userhash) const {
  for (const auto& profile : profiles_) {
    if (profile.userhash == userhash) {
      return &profile;
    }
  }
  return nullptr;
}

const NetworkProfile* NetworkProfileHandler::GetDefaultUserProfile() const {
  for (const auto& profile : profiles_) {
    if (!profile.userhash.empty()) {
      return &profile;
    }
  }
  return nullptr;
}

void NetworkProfileHandler::GetAlwaysOnVpnConfiguration(
    const std::string& profile_path,
    base::OnceCallback<void(std::string, std::string)> callback) {
  ShillProfileClient::Get()->GetProperties(
      dbus::ObjectPath(profile_path),
      base::BindOnce(
          &NetworkProfileHandler::GetAlwaysOnVpnConfigurationCallback,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      base::BindOnce(&LogError, shill::kGetPropertiesFunction, profile_path));
}

void NetworkProfileHandler::GetAlwaysOnVpnConfigurationCallback(
    base::OnceCallback<void(std::string, std::string)> callback,
    base::Value::Dict properties) {
  // A profile always contains the mode.
  std::string* mode = properties.FindString(shill::kAlwaysOnVpnModeProperty);
  DCHECK(mode);
  std::string* service =
      properties.FindString(shill::kAlwaysOnVpnServiceProperty);
  std::move(callback).Run(*mode, service ? *service : std::string());
}

void NetworkProfileHandler::SetAlwaysOnVpnMode(const std::string& profile_path,
                                               const std::string& mode) {
  ShillProfileClient::Get()->SetProperty(
      dbus::ObjectPath(profile_path), shill::kAlwaysOnVpnModeProperty,
      base::Value(mode), base::DoNothing(),
      base::BindOnce(&LogError, shill::kSetPropertyFunction, profile_path));
}

void NetworkProfileHandler::SetAlwaysOnVpnService(
    const std::string& profile_path,
    const std::string& service_path) {
  ShillProfileClient::Get()->SetObjectPathProperty(
      dbus::ObjectPath(profile_path), shill::kAlwaysOnVpnServiceProperty,
      dbus::ObjectPath(service_path), base::DoNothing(),
      base::BindOnce(&LogError, shill::kSetPropertyFunction, profile_path));
}

NetworkProfileHandler::NetworkProfileHandler() {}

void NetworkProfileHandler::Init() {
  ShillManagerClient::Get()->AddPropertyChangedObserver(this);

  // Request the initial profile list.
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&NetworkProfileHandler::GetManagerPropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

NetworkProfileHandler::~NetworkProfileHandler() {
  if (!ShillManagerClient::Get())
    return;

  ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
}

// static
std::unique_ptr<NetworkProfileHandler>
NetworkProfileHandler::InitializeForTesting() {
  auto* handler = new NetworkProfileHandler();
  handler->Init();
  return base::WrapUnique(handler);
}

}  // namespace ash
