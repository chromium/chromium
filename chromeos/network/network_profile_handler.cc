// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_profile_handler.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/network/network_profile_observer.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

bool ConvertListValueToStringVector(const base::ListValue& string_list,
                                    std::vector<std::string>* result) {
  for (size_t i = 0; i < string_list.GetSize(); ++i) {
    std::string str;
    if (!string_list.GetString(i, &str))
      return false;
    result->push_back(str);
  }
  return true;
}

void LogProfileRequestError(const std::string& profile_path,
                            const std::string& error_name,
                            const std::string& error_message) {
  LOG(ERROR) << "Error when requesting properties for profile "
             << profile_path << ": " << error_message;
}

class ProfilePathEquals {
 public:
  explicit ProfilePathEquals(const std::string& path)
      : path_(path) {
  }

  bool operator()(const NetworkProfile& profile) {
    return profile.path == path_;
  }

 private:
  std::string path_;
};

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

void NetworkProfileHandler::GetManagerPropertiesCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (DBUS_METHOD_CALL_FAILURE) {
    LOG(ERROR) << "Error when requesting manager properties.";
    return;
  }

  const base::Value* profiles = NULL;
  properties.GetWithoutPathExpansion(shill::kProfilesProperty, &profiles);
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

  const base::ListValue* profiles_value = NULL;
  value.GetAsList(&profiles_value);
  DCHECK(profiles_value);

  std::vector<std::string> new_profile_paths;
  bool result = ConvertListValueToStringVector(*profiles_value,
                                               &new_profile_paths);
  DCHECK(result);

  VLOG(2) << "Profiles: " << profiles_.size();
  // Search for removed profiles.
  std::vector<std::string> removed_profile_paths;
  for (ProfileList::const_iterator it = profiles_.begin();
       it != profiles_.end(); ++it) {
    if (std::find(new_profile_paths.begin(),
                  new_profile_paths.end(),
                  it->path) == new_profile_paths.end()) {
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
        base::Bind(&NetworkProfileHandler::GetProfilePropertiesCallback,
                   weak_ptr_factory_.GetWeakPtr(), *it),
        base::Bind(&LogProfileRequestError, *it));
  }
}

void NetworkProfileHandler::GetProfilePropertiesCallback(
    const std::string& profile_path,
    const base::DictionaryValue& properties) {
  if (pending_profile_creations_.erase(profile_path) == 0) {
    VLOG(1) << "Ignore received properties, profile was removed.";
    return;
  }
  if (GetProfileForPath(profile_path)) {
    VLOG(1) << "Ignore received properties, profile is already created.";
    return;
  }
  std::string userhash;
  properties.GetStringWithoutPathExpansion(shill::kUserHashProperty, &userhash);

  AddProfile(NetworkProfile(profile_path, userhash));
}

void NetworkProfileHandler::AddProfile(const NetworkProfile& profile) {
  VLOG(2) << "Adding profile " << profile.ToDebugString() << ".";
  profiles_.push_back(profile);
  for (auto& observer : observers_)
    observer.OnProfileAdded(profiles_.back());
}

void NetworkProfileHandler::RemoveProfile(const std::string& profile_path) {
  VLOG(2) << "Removing profile for path " << profile_path << ".";
  ProfileList::iterator found = std::find_if(profiles_.begin(), profiles_.end(),
                                             ProfilePathEquals(profile_path));
  if (found == profiles_.end())
    return;
  NetworkProfile profile = *found;
  profiles_.erase(found);
  for (auto& observer : observers_)
    observer.OnProfileRemoved(profile);
}

const NetworkProfile* NetworkProfileHandler::GetProfileForPath(
    const std::string& profile_path) const {
  ProfileList::const_iterator found =
      std::find_if(profiles_.begin(), profiles_.end(),
                   ProfilePathEquals(profile_path));

  if (found == profiles_.end())
    return NULL;
  return &*found;
}

const NetworkProfile* NetworkProfileHandler::GetProfileForUserhash(
    const std::string& userhash) const {
  for (NetworkProfileHandler::ProfileList::const_iterator it =
           profiles_.begin();
       it != profiles_.end(); ++it) {
    if (it->userhash == userhash)
      return &*it;
  }
  return NULL;
}

const NetworkProfile* NetworkProfileHandler::GetDefaultUserProfile() const {
  for (NetworkProfileHandler::ProfileList::const_iterator it =
           profiles_.begin();
       it != profiles_.end(); ++it) {
    if (!it->userhash.empty())
      return &*it;
  }
  return NULL;
}

NetworkProfileHandler::NetworkProfileHandler() {}

void NetworkProfileHandler::Init() {
  ShillManagerClient::Get()->AddPropertyChangedObserver(this);

  // Request the initial profile list.
  ShillManagerClient::Get()->GetProperties(
      base::Bind(&NetworkProfileHandler::GetManagerPropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

NetworkProfileHandler::~NetworkProfileHandler() {
  ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
}

// static
std::unique_ptr<NetworkProfileHandler>
NetworkProfileHandler::InitializeForTesting() {
  auto* handler = new NetworkProfileHandler();
  handler->Init();
  return base::WrapUnique(handler);
}

}  // namespace chromeos
