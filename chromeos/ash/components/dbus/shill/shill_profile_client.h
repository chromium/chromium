// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_PROFILE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_PROFILE_CLIENT_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_simulated_result.h"
#include "chromeos/ash/components/dbus/shill/shill_client_helper.h"

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace ash {

class ShillPropertyChangedObserver;

// ShillProfileClient is used to communicate with the Shill Profile
// service.  All methods should be called from the origin thread which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) ShillProfileClient {
 public:
  typedef ShillClientHelper::ErrorCallback ErrorCallback;

  // Interface for setting up services for testing. Accessed through
  // GetTestInterface(), only implemented in the stub implementation.
  // TODO(stevenjb): remove dependencies on entry_path -> service_path
  // mappings in some of the TestInterface implementations.
  class TestInterface {
   public:
    virtual void AddProfile(const std::string& profile_path,
                            const std::string& userhash) = 0;

    // Adds an entry to the profile specified by |profile_path|. |properties|
    // must be a dictionary Value of service properties. |entry_path|
    // represents a service path and a corresponding entry will be added to the
    // Manager's kServiceCompleteList property. This will not update the
    // kServicesProperty (which represents 'visible' services).
    virtual void AddEntry(const std::string& profile_path,
                          const std::string& entry_path,
                          const base::Value::Dict& properties) = 0;

    // Adds a service to the profile, copying properties from the
    // ShillServiceClient entry matching |service_path|. Returns false if no
    // Service entry exists or if a Profile entry already exists. Also sets
    // the Profile property of the service in ShillServiceClient.
    virtual bool AddService(const std::string& profile_path,
                            const std::string& service_path) = 0;

    // Copies properties from the ShillServiceClient entry matching
    // |service_path| to the profile entry matching |profile_path|. Returns
    // false if no Service entry exits or if no Profile entry exists.
    virtual bool UpdateService(const std::string& profile_path,
                               const std::string& service_path) = 0;

    // Sets |profiles| to the current list of profile paths.
    virtual void GetProfilePaths(std::vector<std::string>* profiles) = 0;

    // Sets |profiles| to the current list of profile paths that contain an
    // entry for |service_path|.
    virtual void GetProfilePathsContainingService(
        const std::string& service_path,
        std::vector<std::string>* profiles) = 0;

    // Returns the properties contained in the profile matching |profile_path|.
    virtual base::Value::Dict GetProfileProperties(
        const std::string& profile_path) = 0;

    // Returns the entry for |service_path| if it exists in any profile and sets
    // |profile_path| to the path of the profile the service was found in.
    // Profiles are searched starting with the most recently added profile.
    // If the service does not exist in any profile, nullopt is returned.
    virtual std::optional<base::Value::Dict> GetService(
        const std::string& service_path,
        std::string* profile_path) = 0;

    // Returns true iff an entry specified via |service_path| exists in
    // any profile.
    virtual bool HasService(const std::string& service_path) = 0;

    // Remove all profile entries.
    virtual void ClearProfiles() = 0;

    // Makes DeleteEntry succeed, fail, or timeout.
    virtual void SetSimulateDeleteResult(
        FakeShillSimulatedResult delete_result) = 0;

   protected:
    virtual ~TestInterface() = default;
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ShillProfileClient* Get();

  ShillProfileClient(const ShillProfileClient&) = delete;
  ShillProfileClient& operator=(const ShillProfileClient&) = delete;

  // Returns the shared profile path.
  static std::string GetSharedProfilePath();

  // Adds a property changed |observer| for the profile at |profile_path|.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& profile_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Removes a property changed |observer| for the profile at |profile_path|.
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& profile_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Calls the GetProperties DBus method and invokes |callback| on success or
  // |error_callback| on failure. On success |callback| receives a dictionary
  // Value containing the Profile properties.
  virtual void GetProperties(
      const dbus::ObjectPath& profile_path,
      base::OnceCallback<void(base::Value::Dict result)> callback,
      ErrorCallback error_callback) = 0;

  // Calls the SetProperty DBus method to set a property on |profile_path|
  // profile and invokes |callback| on success or |error_callback| on failure.
  virtual void SetProperty(const dbus::ObjectPath& profile_path,
                           const std::string& name,
                           const base::Value& property,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) = 0;

  // Calls the SetProperty DBus method to set an ObjectPath property on
  // |profile_path| profile and invokes |callback| on success or
  // |error_callback| on failure.
  virtual void SetObjectPathProperty(const dbus::ObjectPath& profile_path,
                                     const std::string& name,
                                     const dbus::ObjectPath& property,
                                     base::OnceClosure callback,
                                     ErrorCallback error_callback) = 0;

  // Calls GetEntry method.
  // |callback| is called after the method call succeeds.
  virtual void GetEntry(
      const dbus::ObjectPath& profile_path,
      const std::string& entry_path,
      base::OnceCallback<void(base::Value::Dict result)> callback,
      ErrorCallback error_callback) = 0;

  // Calls DeleteEntry method.
  // |callback| is called after the method call succeeds.
  virtual void DeleteEntry(const dbus::ObjectPath& profile_path,
                           const std::string& entry_path,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) = 0;

  // Returns an interface for testing (stub only), or returns null.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  friend class ShillProfileClientTest;

  // Initialize/Shutdown should be used instead.
  ShillProfileClient();
  virtual ~ShillProfileClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_PROFILE_CLIENT_H_
