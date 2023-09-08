// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IP_PERIPHERAL_IP_PERIPHERAL_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_IP_PERIPHERAL_IP_PERIPHERAL_SERVICE_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// IpPeripheralServiceClient is used to communicate with the DBus interface
// (org.chromium.IpPeripheralService) exposed by the IP Peripheral service on
// Chrome OS devices that support IP-based cameras and other peripherals. The IP
// Peripheral service manages all communications with IP-based peripherals.
//
// All methods should be called from the origin thread (UI thread)
// which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(DBUS_IP_PERIPHERAL_CLIENT) IpPeripheralServiceClient {
 public:
  IpPeripheralServiceClient(const IpPeripheralServiceClient&) = delete;
  IpPeripheralServiceClient& operator=(IpPeripheralServiceClient&) = delete;

  // Callback for the pan/tilt/zoom getter functions.
  using GetCallback = base::OnceCallback<
      void(bool success, int32_t value, int32_t min, int32_t max)>;
  // Callback for the pan/tilt/zoom setter functions.
  using SetCallback = base::OnceCallback<void(bool success)>;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initialize a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static IpPeripheralServiceClient* Get();

  // Getters for pan/tilt/zoom values for an IP camera. The first argument is
  // the IP address of the camera. The callback returns whether or not the call
  // succeeded, the current value, the minimum possible value, and the maximum
  // possible value.
  virtual void GetPan(const std::string& ip, GetCallback callback) = 0;
  virtual void GetTilt(const std::string& ip, GetCallback callback) = 0;
  virtual void GetZoom(const std::string& ip, GetCallback callback) = 0;

  // Setters for pan/tilt/zoom values for an IP camera. Arguments are the IP
  // address of the camera and the pan/tilt/zoom value to be set. The callback
  // returns whether or not the call succeeded. Use the corresponding getter
  // function above to discover the range of valid values.
  virtual void SetPan(const std::string& ip,
                      int32_t pan,
                      SetCallback callback) = 0;
  virtual void SetTilt(const std::string& ip,
                       int32_t tilt,
                       SetCallback callback) = 0;
  virtual void SetZoom(const std::string& ip,
                       int32_t zoom,
                       SetCallback callback) = 0;

  // GetControl and SetControl for UVC XU controls as implemented for an IP
  // camera.  The first argument is the IP address of the camera.  The second
  // argument is the little-endian UVC GUID for the extension unit.  The third
  // argument is the control selector.  In the case of GetControl, the fourth
  // argument is the UVC_GET request.  For SetControl, the fourth argument is
  // a byte vector providing the control setting.  The callback returns whether
  // the dbus call succeeded and for GetControl, the byte vector result.
  using GetControlCallback =
      base::OnceCallback<void(bool success, std::vector<uint8_t> result)>;
  using SetControlCallback = base::OnceCallback<void(bool success)>;
  virtual void GetControl(const std::string& ip,
                          const std::vector<uint8_t>& guid_le,
                          uint8_t control_selector,
                          uint8_t uvc_get_request,
                          GetControlCallback callback) = 0;
  virtual void SetControl(const std::string& ip,
                          const std::vector<uint8_t>& guid_le,
                          uint8_t control_selector,
                          const std::vector<uint8_t>& control_setting,
                          SetControlCallback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  IpPeripheralServiceClient();
  virtual ~IpPeripheralServiceClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IP_PERIPHERAL_IP_PERIPHERAL_SERVICE_CLIENT_H_
