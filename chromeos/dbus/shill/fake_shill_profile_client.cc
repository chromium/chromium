// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill/fake_shill_profile_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/adapters.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

struct FakeShillProfileClient::ProfileProperties {
  std::string path;                  // Profile path
  base::DictionaryValue entries;     // Dictionary of Service Dictionaries
  base::DictionaryValue properties;  // Dictionary of Profile properties
};

namespace {

void PassDictionary(
    const ShillProfileClient::DictionaryValueCallbackWithoutStatus& callback,
    const base::DictionaryValue* dictionary) {
  callback.Run(*dictionary);
}

}  // namespace

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
    const DictionaryValueCallbackWithoutStatus& callback,
    const ErrorCallback& error_callback) {
  ProfileProperties* profile = GetProfile(profile_path, error_callback);
  if (!profile)
    return;

  auto entry_paths = std::make_unique<base::ListValue>();
  for (base::DictionaryValue::Iterator it(profile->entries); !it.IsAtEnd();
       it.Advance()) {
    entry_paths->AppendString(it.key());
  }

  std::unique_ptr<base::DictionaryValue> properties =
      profile->properties.CreateDeepCopy();
  properties->SetWithoutPathExpansion(shill::kEntriesProperty,
                                      std::move(entry_paths));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PassDictionary, callback,
                                base::Owned(properties.release())));
}

void FakeShillProfileClient::GetEntry(
    const dbus::ObjectPath& profile_path,
    const std::string& entry_path,
    const DictionaryValueCallbackWithoutStatus& callback,
    const ErrorCallback& error_callback) {
  ProfileProperties* profile = GetProfile(profile_path, error_callback);
  if (!profile)
    return;

  base::DictionaryValue* entry = nullptr;
  profile->entries.GetDictionaryWithoutPathExpansion(entry_path, &entry);
  if (!entry) {
    error_callback.Run("Error.InvalidProfileEntry", "Invalid profile entry");
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PassDictionary, callback,
                                base::Owned(entry->DeepCopy())));
}

void FakeShillProfileClient::DeleteEntry(const dbus::ObjectPath& profile_path,
                                         const std::string& entry_path,
                                         const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  switch (simulate_delete_result_) {
    case FakeShillSimulatedResult::kSuccess:
      break;
    case FakeShillSimulatedResult::kFailure:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(error_callback, "Error", "Simulated failure"));
      return;
    case FakeShillSimulatedResult::kTimeout:
      // No callbacks get executed and the caller should eventually timeout.
      return;
  }

  ProfileProperties* profile = GetProfile(profile_path, error_callback);
  if (!profile) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(error_callback, "Error.InvalidProfile",
                                  profile_path.value()));
    return;
  }

  if (!profile->entries.RemoveWithoutPathExpansion(entry_path, nullptr)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(error_callback, "Error.InvalidProfileEntry",
                                  entry_path));
    return;
  }

  ShillServiceClient::Get()
      ->GetTestInterface()
      ->ClearConfiguredServiceProperties(entry_path);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

ShillProfileClient::TestInterface* FakeShillProfileClient::GetTestInterface() {
  return this;
}

void FakeShillProfileClient::AddProfile(const std::string& profile_path,
                                        const std::string& userhash) {
  if (GetProfile(dbus::ObjectPath(profile_path), ErrorCallback()))
    return;

  // If adding a shared profile, make sure there are no user profiles currently
  // on the stack - this assumes there is at most one shared profile.
  CHECK(profile_path != GetSharedProfilePath() || profiles_.empty())
      << "Shared profile must be added before any user profile.";

  auto profile = std::make_unique<ProfileProperties>();
  profile->properties.SetKey(shill::kUserHashProperty, base::Value(userhash));
  profile->path = profile_path;
  profiles_.emplace_back(std::move(profile));

  ShillManagerClient::Get()->GetTestInterface()->AddProfile(profile_path);
}

void FakeShillProfileClient::AddEntry(const std::string& profile_path,
                                      const std::string& entry_path,
                                      const base::DictionaryValue& properties) {
  ProfileProperties* profile =
      GetProfile(dbus::ObjectPath(profile_path), ErrorCallback());
  DCHECK(profile);
  profile->entries.SetKey(entry_path, properties.Clone());
  ShillManagerClient::Get()->GetTestInterface()->AddManagerService(entry_path,
                                                                   true);
}

bool FakeShillProfileClient::AddService(const std::string& profile_path,
                                        const std::string& service_path) {
  ProfileProperties* profile =
      GetProfile(dbus::ObjectPath(profile_path), ErrorCallback());
  if (!profile) {
    LOG(ERROR) << "AddService: No matching profile: " << profile_path
               << " for: " << service_path;
    return false;
  }
  if (profile->entries.HasKey(service_path))
    return false;
  return AddOrUpdateServiceImpl(profile_path, service_path, profile);
}

bool FakeShillProfileClient::UpdateService(const std::string& profile_path,
                                           const std::string& service_path) {
  ProfileProperties* profile =
      GetProfile(dbus::ObjectPath(profile_path), ErrorCallback());
  if (!profile) {
    LOG(ERROR) << "UpdateService: No matching profile: " << profile_path
               << " for: " << service_path;
    return false;
  }
  if (!profile->entries.HasKey(service_path)) {
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
  const base::DictionaryValue* service_properties =
      service_test->GetServiceProperties(service_path);
  if (!service_properties) {
    LOG(ERROR) << "No matching service: " << service_path;
    return false;
  }
  std::string service_profile_path;
  service_properties->GetStringWithoutPathExpansion(shill::kProfileProperty,
                                                    &service_profile_path);
  if (service_profile_path.empty()) {
    base::Value profile_path_value(profile_path);
    service_test->SetServiceProperty(service_path, shill::kProfileProperty,
                                     profile_path_value);
  } else if (service_profile_path != profile_path) {
    LOG(ERROR) << "Service has non matching profile path: "
               << service_profile_path;
    return false;
  }

  profile->entries.SetKey(service_path, service_properties->Clone());
  return true;
}

void FakeShillProfileClient::GetProfilePaths(
    std::vector<std::string>* profiles) {
  for (const auto& profile : profiles_)
    profiles->push_back(profile->path);
}

void FakeShillProfileClient::GetProfilePathsContainingService(
    const std::string& service_path,
    std::vector<std::string>* profiles) {
  for (const auto& profile : profiles_) {
    if (profile->entries.FindKeyOfType(service_path,
                                       base::Value::Type::DICTIONARY)) {
      profiles->push_back(profile->path);
    }
  }
}

bool FakeShillProfileClient::GetService(const std::string& service_path,
                                        std::string* profile_path,
                                        base::DictionaryValue* properties) {
  DCHECK(profile_path);
  DCHECK(properties);

  properties->Clear();
  // Returns the entry added latest.
  for (const auto& profile : base::Reversed(profiles_)) {
    const base::Value* entry = profile->entries.FindKeyOfType(
        service_path, base::Value::Type::DICTIONARY);
    if (entry) {
      *profile_path = profile->path;
      *properties = static_cast<base::DictionaryValue&&>(entry->Clone());
      return true;
    }
  }
  return false;
}

bool FakeShillProfileClient::HasService(const std::string& service_path) {
  for (const auto& profile : profiles_) {
    if (profile->entries.FindKeyOfType(service_path,
                                       base::Value::Type::DICTIONARY)) {
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
    const dbus::ObjectPath& profile_path,
    const ErrorCallback& error_callback) {
  for (auto& profile : profiles_) {
    if (profile->path == profile_path.value())
      return profile.get();
  }
  if (!error_callback.is_null())
    error_callback.Run("Error.InvalidProfile", "Invalid profile");
  return nullptr;
}

}  // namespace chromeos
