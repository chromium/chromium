// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/fake_shill_profile_client.h"

#include <memory>
#include <utility>

#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

struct FakeShillProfileClient::ProfileProperties {
  std::string profile_path;
  // Dictionary of Service Dictionaries
  base::Value::Dict entries;
  // Dictionary of Profile properties
  base::Value::Dict properties;
};

FakeShillProfileClient::FakeShillProfileClient() = default;

FakeShillProfileClient::~FakeShillProfileClient() = default;

void FakeShillProfileClient::AddPropertyChangedObserver(
    const dbus::ObjectPath& profile_path,
    ShillPropertyChangedObserver* observer) {}

void FakeShillProfileClient::RemovePropertyChangedObserver(
    const dbus::ObjectPath& profile_path,
    ShillPropertyChangedObserver* observer) {}

void FakeShillProfileClient::GetProperties(
    const dbus::ObjectPath& profile_path,
    base::OnceCallback<void(base::Value::Dict result)> callback,
    ErrorCallback error_callback) {
  ProfileProperties* profile = GetProfile(profile_path);
  if (!profile) {
    std::move(error_callback).Run("Error.InvalidProfile", "Invalid profile");
    return;
  }

  base::Value::List entry_paths;
  for (const auto it : profile->entries) {
    entry_paths.Append(it.first);
  }

  base::Value::Dict properties = profile->properties.Clone();
  properties.Set(shill::kEntriesProperty, std::move(entry_paths));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(properties)));
}

void FakeShillProfileClient::SetProperty(const dbus::ObjectPath& profile_path,
                                         const std::string& name,
                                         const base::Value& property,
                                         base::OnceClosure callback,
                                         ErrorCallback error_callback) {
  ProfileProperties* profile = GetProfile(profile_path);
  if (!profile) {
    std::move(error_callback).Run("Error.InvalidProfile", "Invalid profile");
    return;
  }
  profile->properties.Set(name, property.Clone());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void FakeShillProfileClient::SetObjectPathProperty(
    const dbus::ObjectPath& profile_path,
    const std::string& name,
    const dbus::ObjectPath& property,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  ProfileProperties* profile = GetProfile(profile_path);
  if (!profile) {
    std::move(error_callback).Run("Error.InvalidProfile", "Invalid profile");
    return;
  }
  profile->properties.Set(name, property.value());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void FakeShillProfileClient::GetEntry(
    const dbus::ObjectPath& profile_path,
    const std::string& entry_path,
    base::OnceCallback<void(base::Value::Dict result)> callback,
    ErrorCallback error_callback) {
  ProfileProperties* profile = GetProfile(profile_path);
  if (!profile) {
    std::move(error_callback).Run("Error.InvalidProfile", "Invalid profile");
    return;
  }

  const base::Value::Dict* entry = profile->entries.FindDict(entry_path);
  if (!entry) {
    std::move(error_callback)
        .Run("Error.InvalidProfileEntry", "Invalid profile entry");
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), entry->Clone()));
}

void FakeShillProfileClient::DeleteEntry(const dbus::ObjectPath& profile_path,
                                         const std::string& entry_path,
                                         base::OnceClosure callback,
                                         ErrorCallback error_callback) {
  switch (simulate_delete_result_) {
    case FakeShillSimulatedResult::kSuccess:
      break;
    case FakeShillSimulatedResult::kFailure:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(error_callback), "Error",
                                    "Simulated failure"));
      return;
    case FakeShillSimulatedResult::kTimeout:
      // No callbacks get executed and the caller should eventually timeout.
      return;
    case FakeShillSimulatedResult::kInProgress:
      // No callbacks get executed in this case.
      return;
  }

  ProfileProperties* profile = GetProfile(profile_path);
  if (!profile) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback), "Error.InvalidProfile",
                       profile_path.value()));
    return;
  }

  if (!profile->entries.Remove(entry_path)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  "Error.InvalidProfileEntry", entry_path));
    return;
  }

  ShillServiceClient::Get()
      ->GetTestInterface()
      ->ClearConfiguredServiceProperties(entry_path);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

ShillProfileClient::TestInterface* FakeShillProfileClient::GetTestInterface() {
  return this;
}

void FakeShillProfileClient::AddProfile(const std::string& profile_path,
                                        const std::string& userhash) {
  if (GetProfile(dbus::ObjectPath(profile_path)))
    return;

  // If adding a shared profile, make sure there are no user profiles currently
  // on the stack - this assumes there is at most one shared profile.
  CHECK(profile_path != GetSharedProfilePath() || profiles_.empty())
      << "Shared profile must be added before any user profile.";

  ProfileProperties profile;
  profile.properties.Set(shill::kUserHashProperty, userhash);
  profile.profile_path = profile_path;
  profiles_.push_back(std::move(profile));

  ShillManagerClient::Get()->GetTestInterface()->AddProfile(profile_path);
}

void FakeShillProfileClient::AddEntry(const std::string& profile_path,
                                      const std::string& entry_path,
                                      const base::Value::Dict& properties) {
  ProfileProperties* profile = GetProfile(dbus::ObjectPath(profile_path));
  DCHECK(profile);
  profile->entries.Set(entry_path, properties.Clone());
  ShillManagerClient::Get()->GetTestInterface()->AddManagerService(entry_path,
                                                                   true);
}

bool FakeShillProfileClient::AddService(const std::string& profile_path,
                                        const std::string& service_path) {
  ProfileProperties* profile = GetProfile(dbus::ObjectPath(profile_path));
  if (!profile) {
    LOG(ERROR) << "AddService: No matching profile: " << profile_path
               << " for: " << service_path;
    return false;
  }
  if (profile->entries.contains(service_path)) {
    return false;
  }
  return AddOrUpdateServiceImpl(profile_path, service_path, profile);
}

bool FakeShillProfileClient::UpdateService(const std::string& profile_path,
                                           const std::string& service_path) {
  ProfileProperties* profile = GetProfile(dbus::ObjectPath(profile_path));
  if (!profile) {
    LOG(ERROR) << "UpdateService: No matching profile: " << profile_path
               << " for: " << service_path;
    return false;
  }
  if (!profile->entries.contains(service_path)) {
    LOG(ERROR) << "UpdateService: Profile: " << profile_path
               << " does not contain Service: " << service_path;
    return false;
  }
  return AddOrUpdateServiceImpl(profile_path, service_path, profile);
}

bool FakeShillProfileClient::AddOrUpdateServiceImpl(
    const std::string& profile_path,
    const std::string& service_path,
    ProfileProperties* profile) {
  ShillServiceClient::TestInterface* service_test =
      ShillServiceClient::Get()->GetTestInterface();
  const base::Value::Dict* service_properties =
      service_test->GetServiceProperties(service_path);
  if (!service_properties) {
    LOG(ERROR) << "No matching service: " << service_path;
    return false;
  }
  const std::string* service_profile_path =
      service_properties->FindString(shill::kProfileProperty);
  if (!service_profile_path || service_profile_path->empty()) {
    base::Value profile_path_value(profile_path);
    service_test->SetServiceProperty(service_path, shill::kProfileProperty,
                                     profile_path_value);
  } else if (*service_profile_path != profile_path) {
    LOG(ERROR) << "Service has non matching profile path: "
               << *service_profile_path;
    return false;
  }

  profile->entries.Set(service_path, service_properties->Clone());
  return true;
}

void FakeShillProfileClient::GetProfilePaths(
    std::vector<std::string>* profiles) {
  for (const auto& profile : profiles_)
    profiles->push_back(profile.profile_path);
}

void FakeShillProfileClient::GetProfilePathsContainingService(
    const std::string& service_path,
    std::vector<std::string>* profiles) {
  for (const auto& profile : profiles_) {
    if (profile.entries.FindDict(service_path)) {
      profiles->push_back(profile.profile_path);
    }
  }
}

base::Value::Dict FakeShillProfileClient::GetProfileProperties(
    const std::string& profile_path) {
  ProfileProperties* profile = GetProfile(dbus::ObjectPath(profile_path));
  DCHECK(profile);
  return profile->properties.Clone();
}

std::optional<base::Value::Dict> FakeShillProfileClient::GetService(
    const std::string& service_path,
    std::string* profile_path) {
  DCHECK(profile_path);

  // Returns the entry added latest.
  for (const auto& profile : base::Reversed(profiles_)) {
    const base::Value::Dict* entry = profile.entries.FindDict(service_path);
    if (!entry) {
      continue;
    }
    *profile_path = profile.profile_path;
    return entry->Clone();
  }
  return std::nullopt;
}

bool FakeShillProfileClient::HasService(const std::string& service_path) {
  for (const auto& profile : profiles_) {
    if (profile.entries.FindDict(service_path)) {
      return true;
    }
  }

  return false;
}

void FakeShillProfileClient::ClearProfiles() {
  profiles_.clear();
}

void FakeShillProfileClient::SetSimulateDeleteResult(
    FakeShillSimulatedResult delete_result) {
  simulate_delete_result_ = delete_result;
}

FakeShillProfileClient::ProfileProperties* FakeShillProfileClient::GetProfile(
    const dbus::ObjectPath& profile_path) {
  for (auto& profile : profiles_) {
    if (profile.profile_path == profile_path.value())
      return &profile;
  }
  return nullptr;
}

}  // namespace ash
