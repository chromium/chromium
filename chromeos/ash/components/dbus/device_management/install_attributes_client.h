// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_DEVICE_MANAGEMENT_INSTALL_ATTRIBUTES_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_DEVICE_MANAGEMENT_INSTALL_ATTRIBUTES_CLIENT_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/device_management/device_management_interface.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace dbus {
class Bus;
}

namespace ash {

// InstallAttributesClient is used to communicate with the
// org.chromium.InstallAttributes interface within org.chromium.DeviceManagement
// service exposed by device_managementd. All method should be called from the origin
// thread (UI thread) which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(DEVICE_MANAGEMENT_CLIENT) InstallAttributesClient {
 public:
  using InstallAttributesGetCallback =
      chromeos::DBusMethodCallback<::device_management::InstallAttributesGetReply>;
  using InstallAttributesSetCallback =
      chromeos::DBusMethodCallback<::device_management::InstallAttributesSetReply>;
  using InstallAttributesFinalizeCallback = chromeos::DBusMethodCallback<
      ::device_management::InstallAttributesFinalizeReply>;
  using InstallAttributesGetStatusCallback = chromeos::DBusMethodCallback<
      ::device_management::InstallAttributesGetStatusReply>;
  using RemoveFirmwareManagementParametersCallback =
      chromeos::DBusMethodCallback<
          ::device_management::RemoveFirmwareManagementParametersReply>;
  using SetFirmwareManagementParametersCallback = chromeos::DBusMethodCallback<
      ::device_management::SetFirmwareManagementParametersReply>;
  using GetFirmwareManagementParametersCallback = chromeos::DBusMethodCallback<
      ::device_management::GetFirmwareManagementParametersReply>;

  // Not copyable or movable.
  InstallAttributesClient(const InstallAttributesClient&) = delete;
  InstallAttributesClient& operator=(const InstallAttributesClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static InstallAttributesClient* Get();

  // Actual DBus Methods:

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) = 0;

  // Retrieves an install attribute.
  virtual void InstallAttributesGet(
      const ::device_management::InstallAttributesGetRequest& request,
      InstallAttributesGetCallback callback) = 0;

  // Finalizes the install attribute.
  virtual void InstallAttributesFinalize(
      const ::device_management::InstallAttributesFinalizeRequest& request,
      InstallAttributesFinalizeCallback callback) = 0;

  // Get the current status of the install attributes.
  virtual void InstallAttributesGetStatus(
      const ::device_management::InstallAttributesGetStatusRequest& request,
      InstallAttributesGetStatusCallback callback) = 0;

  // Removes/unset the firmware management parameters.
  virtual void RemoveFirmwareManagementParameters(
      const ::device_management::RemoveFirmwareManagementParametersRequest&
          request,
      RemoveFirmwareManagementParametersCallback callback) = 0;

  // Set the firmware management parameters.
  virtual void SetFirmwareManagementParameters(
      const ::device_management::SetFirmwareManagementParametersRequest& request,
      SetFirmwareManagementParametersCallback callback) = 0;

  // Get the firmware management parameters.
  virtual void GetFirmwareManagementParameters(
      const ::device_management::GetFirmwareManagementParametersRequest& request,
      GetFirmwareManagementParametersCallback callback) = 0;

  // Blocking version of InstallAttributesGet().
  virtual std::optional<::device_management::InstallAttributesGetReply>
  BlockingInstallAttributesGet(
      const ::device_management::InstallAttributesGetRequest& request) = 0;

  // Blocking version of InstallAttributesSet().
  virtual std::optional<::device_management::InstallAttributesSetReply>
  BlockingInstallAttributesSet(
      const ::device_management::InstallAttributesSetRequest& request) = 0;

  // Blocking version of InstallAttributesFinalize().
  virtual std::optional<::device_management::InstallAttributesFinalizeReply>
  BlockingInstallAttributesFinalize(
      const ::device_management::InstallAttributesFinalizeRequest& request) = 0;

  // Blocking version of InstallAttributesGetStatus().
  virtual std::optional<::device_management::InstallAttributesGetStatusReply>
  BlockingInstallAttributesGetStatus(
      const ::device_management::InstallAttributesGetStatusRequest& request) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  InstallAttributesClient();
  virtual ~InstallAttributesClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_DEVICE_MANAGEMENT_INSTALL_ATTRIBUTES_CLIENT_H_
