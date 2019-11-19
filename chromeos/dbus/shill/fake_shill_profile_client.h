// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_FAKE_SHILL_PROFILE_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_FAKE_SHILL_PROFILE_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"

namespace chromeos {

// A stub implementation of ShillProfileClient.
class COMPONENT_EXPORT(SHILL_CLIENT) FakeShillProfileClient
    : public ShillProfileClient,
      public ShillProfileClient::TestInterface {
 public:
  FakeShillProfileClient();
  ~FakeShillProfileClient() override;

  // ShillProfileClient overrides
  void AddPropertyChangedObserver(
      const dbus::ObjectPath& profile_path,
      ShillPropertyChangedObserver* observer) override;
  void RemovePropertyChangedObserver(
      const dbus::ObjectPath& profile_path,
      ShillPropertyChangedObserver* observer) override;
  void GetProperties(const dbus::ObjectPath& profile_path,
                     const DictionaryValueCallbackWithoutStatus& callback,
                     const ErrorCallback& error_callback) override;
  void GetEntry(const dbus::ObjectPath& profile_path,
                const std::string& entry_path,
                const DictionaryValueCallbackWithoutStatus& callback,
                const ErrorCallback& error_callback) override;
  void DeleteEntry(const dbus::ObjectPath& profile_path,
                   const std::string& entry_path,
                   const base::Closure& callback,
                   const ErrorCallback& error_callback) override;
  ShillProfileClient::TestInterface* GetTestInterface() override;

  // ShillProfileClient::TestInterface overrides.
  void AddProfile(const std::string& profile_path,
                  const std::string& userhash) override;
  void AddEntry(const std::string& profile_path,
                const std::string& entry_path,
                const base::DictionaryValue& properties) override;
  bool AddService(const std::string& profile_path,
                  const std::string& service_path) override;
  bool UpdateService(const std::string& profile_path,
                     const std::string& service_path) override;
  void GetProfilePaths(std::vector<std::string>* profiles) override;
  void GetProfilePathsContainingService(
      const std::string& service_path,
      std::vector<std::string>* profiles) override;
  bool GetService(const std::string& service_path,
                  std::string* profile_path,
                  base::DictionaryValue* properties) override;
  bool HasService(const std::string& service_path) override;
  void ClearProfiles() override;
  void SetSimulateDeleteResult(FakeShillSimulatedResult delete_result) override;

 private:
  struct ProfileProperties;

  bool AddOrUpdateServiceImpl(const std::string& profile_path,
                              const std::string& service_path,
                              ProfileProperties* profile);

  ProfileProperties* GetProfile(const dbus::ObjectPath& profile_path,
                                const ErrorCallback& error_callback);

  // List of profiles known to the client in order they were added, and in the
  // reverse order of priority.
  // |AddProfile| will encure that shared profile is never added after a user
  // profile.
  std::vector<std::unique_ptr<ProfileProperties>> profiles_;

  FakeShillSimulatedResult simulate_delete_result_ =
      FakeShillSimulatedResult::kSuccess;

  DISALLOW_COPY_AND_ASSIGN(FakeShillProfileClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_FAKE_SHILL_PROFILE_CLIENT_H_
