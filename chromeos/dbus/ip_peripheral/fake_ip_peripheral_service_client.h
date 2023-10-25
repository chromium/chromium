// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IP_PERIPHERAL_FAKE_IP_PERIPHERAL_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_IP_PERIPHERAL_FAKE_IP_PERIPHERAL_SERVICE_CLIENT_H_

#include <string>

#include "chromeos/dbus/ip_peripheral/ip_peripheral_service_client.h"

namespace chromeos {

class COMPONENT_EXPORT(DBUS_IP_PERIPHERAL_CLIENT) FakeIpPeripheralServiceClient
    : public IpPeripheralServiceClient {
 public:
  FakeIpPeripheralServiceClient();
  FakeIpPeripheralServiceClient(const FakeIpPeripheralServiceClient&) = delete;
  FakeIpPeripheralServiceClient& operator=(
      const FakeIpPeripheralServiceClient&) = delete;
  ~FakeIpPeripheralServiceClient() override;

  // Checks that FakeIpPeripheralServiceClient was initialized and returns it.
  static FakeIpPeripheralServiceClient* Get();

  // IpPeripheralServiceClient
  void GetPan(const std::string& ip, GetCallback callback) override;
  void GetTilt(const std::string& ip, GetCallback callback) override;
  void GetZoom(const std::string& ip, GetCallback callback) override;
  void SetPan(const std::string& ip,
              int32_t pan,
              SetCallback callback) override;
  void SetTilt(const std::string& ip,
               int32_t tilt,
               SetCallback callback) override;
  void SetZoom(const std::string& ip,
               int32_t zoom,
               SetCallback callback) override;
  void GetControl(const std::string& ip,
                  const std::vector<uint8_t>& guid_le,
                  uint8_t control_selector,
                  uint8_t uvc_get_request,
                  GetControlCallback callback) override;
  void SetControl(const std::string& ip,
                  const std::vector<uint8_t>& guid_le,
                  uint8_t control_selector,
                  const std::vector<uint8_t>& control_setting,
                  SetControlCallback callback) override;

  int get_pan_call_count() const { return get_pan_call_count_; }
  int get_tilt_call_count() const { return get_tilt_call_count_; }
  int get_zoom_call_count() const { return get_zoom_call_count_; }
  int set_pan_call_count() const { return set_pan_call_count_; }
  int set_tilt_call_count() const { return set_tilt_call_count_; }
  int set_zoom_call_count() const { return set_zoom_call_count_; }
  int get_control_call_count() const { return get_control_call_count_; }
  int set_control_call_count() const { return set_control_call_count_; }

  int32_t pan() const { return pan_; }
  void set_pan(int32_t pan) { pan_ = pan; }
  int32_t tilt() const { return tilt_; }
  void set_tilt(int32_t tilt) { tilt_ = tilt; }
  int32_t zoom() const { return zoom_; }
  void set_zoom(int32_t zoom) { zoom_ = zoom; }
  std::vector<uint8_t> control_response() const { return control_response_; }
  void set_control_response(std::vector<uint8_t> control_setting) {
    control_response_ = control_setting;
  }

 private:
  int get_pan_call_count_ = 0;
  int get_tilt_call_count_ = 0;
  int get_zoom_call_count_ = 0;
  int set_pan_call_count_ = 0;
  int set_tilt_call_count_ = 0;
  int set_zoom_call_count_ = 0;
  int32_t pan_ = 0;
  int32_t tilt_ = 0;
  int32_t zoom_ = 0;
  int get_control_call_count_ = 0;
  int set_control_call_count_ = 0;
  std::vector<uint8_t> control_response_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IP_PERIPHERAL_FAKE_IP_PERIPHERAL_SERVICE_CLIENT_H_
