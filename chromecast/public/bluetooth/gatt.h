// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_BLUETOOTH_GATT_H_
#define CHROMECAST_PUBLIC_BLUETOOTH_GATT_H_

#include <vector>

#include "bluetooth_types.h"
#include "chromecast_export.h"

namespace chromecast {
namespace bluetooth_v2_shlib {

// GATT (Generic Attributes) is the primary mechanism in BLE (Bluetooth Low
// Energy) for transmiting data.
class CHROMECAST_EXPORT Gatt {
 public:
  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html
  enum Permissions : uint16_t {
    PERMISSION_NONE = 0,
    PERMISSION_READ = 1 << 0,
    PERMISSION_READ_ENCRYPTED = 1 << 1,
    PERMISSION_READ_ENCRYPTED_MITM = 1 << 2,
    PERMISSION_WRITE = 1 << 4,
    PERMISSION_WRITE_ENCRYPTED = 1 << 5,
    PERMISSION_WRITE_ENCRYPTED_MITM = 1 << 6,
    PERMISSION_WRITE_SIGNED = 1 << 7,
    PERMISSION_WRITE_SIGNED_MITM = 1 << 8,
  };

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html
  enum Properties : uint8_t {
    PROPERTY_NONE = 0,
    PROPERTY_BROADCAST = 1 << 0,
    PROPERTY_READ = 1 << 1,
    PROPERTY_WRITE_NO_RESPONSE = 1 << 2,
    PROPERTY_WRITE = 1 << 3,
    PROPERTY_NOTIFY = 1 << 4,
    PROPERTY_INDICATE = 1 << 5,
    PROPERTY_SIGNED_WRITE = 1 << 6,
    PROPERTY_EXTENDED_PROPS = 1 << 7,
  };

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html
  enum WriteType : uint8_t {
    WRITE_TYPE_NONE = 0,
    WRITE_TYPE_NO_RESPONSE = 1 << 0,
    WRITE_TYPE_DEFAULT = 1 << 1,
    WRITE_TYPE_SIGNED = 1 << 2,
  };

  // Core 4.2 Vol3 part F 3.4.1.1 Error Response
  enum class Status {
    NONE = 0,
    INVALID_HANDLE = 0x01,
    READ_NOT_PERMITTED = 0x02,
    WRITE_NOT_PERMITTED = 0x03,
    INVALID_PDU = 0x04,
    INSUFFICIENT_AUTHEN = 0x05,
    REQUEST_NOT_SUPPORTED = 0x06,
    INVALID_OFFSET = 0x07,
    INSUFFICIENT_AUTHOR = 0x08,
    PREP_QUEUE_FULL = 0x09,
    ATTRIBUTE_NOT_FOUND = 0x0a,
    ATTRIBUTE_NOT_LONG = 0x0b,
    INSUFFICIENT_KEY_SIZE = 0x0c,
    INVALID_ATTRIBUTE_LENGTH = 0x0d,
    UNLIKELY = 0x0e,
    INSUFFICIENT_ENCR = 0x0f,
    UNSUPPORTED_GRP_TYPE = 0x10,
    INSUFFICIENT_RESOURCES = 0x11,
    CCCD_IMPROPERLY_CONFIGURED = 0xFD,
    PROCEDURE_IN_PROGRESS = 0xFE,
    OUT_OF_RANGE = 0xFF
  };

  // Attributes that describe a characteristic value.
  struct Descriptor {
    Uuid uuid;
    uint16_t handle;
    Permissions permissions;
  };

  // Attribute types that contain a single logical value.
  struct Characteristic {
    Characteristic();
    Characteristic(const Characteristic& other);
    ~Characteristic();

    Uuid uuid;
    uint16_t handle;
    Permissions permissions;
    Properties properties;
    std::vector<Descriptor> descriptors;
  };

  // Services are collections of characteristics and relationships to other
  // services that encapsulate the behavior of part of a device.
  struct Service {
    Service();
    Service(const Service& other);
    ~Service();
    Uuid uuid;
    uint16_t handle;
    bool primary;
    std::vector<Characteristic> characteristics;
    std::vector<Service> included_services;
  };

  // The GATT client role is when a device connects to a remote GATT server.
  class Client {
   public:
    // See frameworks/base/core/java/android/bluetooth/BluetoothGatt.java
    // AUTHENTICATION_{NONE,NO_MITM,MITM}
    enum AuthReq : int32_t {
      AUTH_REQ_INVALID = -1,
      AUTH_REQ_NONE = 0,
      AUTH_REQ_NO_MITM = 1,
      AUTH_REQ_MITM = 2,

      AUTH_REQ_MAX = AUTH_REQ_MITM,
    };

    enum class Transport {
      kAuto,
      kBrEdr,
      kLe,
    };

    // These callbacks may be on any thead.
    class Delegate {
     public:
      // Called when the connection changes.
      virtual void OnConnectChanged(const Addr& addr,
                                    bool status,
                                    bool connected) = 0;

      // Called when the bonding state changes.
      virtual void OnBondChanged(const Addr& addr,
                                 bool status,
                                 bool bonded) = 0;

      // Called on a Characteristic value notification.
      virtual void OnNotification(const Addr& addr,
                                  uint16_t handle,
                                  const std::vector<uint8_t>& value) = 0;

      // Called in response to ReadCharacteristic.
      virtual void OnCharacteristicReadResponse(
          const Addr& addr,
          bool status,
          uint16_t handle,
          const std::vector<uint8_t>& value) = 0;

      // Called in response to WriteCharacteristic.
      virtual void OnCharacteristicWriteResponse(const Addr& addr,
                                                 bool status,
                                                 uint16_t handle) = 0;

      // Called in response to ReadDescriptor.
      virtual void OnDescriptorReadResponse(
          const Addr& addr,
          bool status,
          uint16_t handle,
          const std::vector<uint8_t>& value) = 0;

      // Called in response to WriteDescriptor.
      virtual void OnDescriptorWriteResponse(const Addr& addr,
                                             bool status,
                                             uint16_t handle) = 0;

      // Called in response to ReadRemoteRssi.
      virtual void OnReadRemoteRssi(const Addr& addr,
                                    bool status,
                                    int rssi) = 0;

      // Called when the connection MTU changes.
      virtual void OnMtuChanged(const Addr& addr, bool status, int mtu) = 0;

      // Called when the service list is obtained.
      virtual void OnGetServices(const Addr& addr,
                                 const std::vector<Service>& services) = 0;

      // Called when services are removed.
      virtual void OnServicesRemoved(const Addr& addr,
                                     uint16_t start_handle,
                                     uint16_t end_handle) = 0;

      // Called when services are added.
      virtual void OnServicesAdded(const Addr& addr,
                                   const std::vector<Service>& services) = 0;

      virtual ~Delegate() = default;
    };

    // Returns true if GATT client profile is supported.
    static bool IsSupported();
    static void SetDelegate(Delegate* delegate);

    // Create a connection to remote device |addr| using |transport|.
    static bool Connect(const Addr& addr, Transport transport);

    // Remove connection to remote device |addr|.
    static bool Disconnect(const Addr& addr);

    // Create bond to remote device |addr|.
    static bool CreateBond(const Addr& addr);

    // Remove bond to remote device |addr|.
    static bool RemoveBond(const Addr& addr);

    // Read |characteristic| from remote device |addr|. If |auth_req| is
    // AUTH_REQ_INVALID, this function will automatically retry stronger
    // authentications on failure.
    static bool ReadCharacteristic(const Addr& addr,
                                   const Characteristic& characteristic,
                                   AuthReq auth_req);

    // Write |characteristic| on remote device |addr| with |write_type|. If
    // |auth_req| is AUTH_REQ_INVALID, this function will automatically retry
    // stronger authentications on failure.
    static bool WriteCharacteristic(const Addr& addr,
                                    const Characteristic& characteristic,
                                    AuthReq auth_req,
                                    WriteType write_type,
                                    const std::vector<uint8_t>& value);

    // Read |descriptor| from remote device |addr|. If |auth_req| is
    // AUTH_REQ_INVALID, this function will automatically retry stronger
    // authentications on failure.
    static bool ReadDescriptor(const Addr& addr,
                               const Descriptor& descriptor,
                               AuthReq auth_req);

    // Write |descriptor| on remote device |addr|. If |auth_req| is
    // AUTH_REQ_INVALID, this function will automatically retry stronger
    // authentications on failure.
    static bool WriteDescriptor(const Addr& addr,
                                const Descriptor& descriptor,
                                AuthReq auth_req,
                                const std::vector<uint8_t>& value);

    // Register or deregister for notifications of |characteristic| on device
    // |addr|.
    static bool SetCharacteristicNotification(
        const Addr& addr,
        const Characteristic& characteristic,
        bool enable);

    // Read the RSSI of remote device with |addr|
    static bool ReadRemoteRssi(const Addr& addr);

    // Request |mtu| on the connection with device |addr|.
    static bool RequestMtu(const Addr& addr, int mtu);

    // Update connection parameters with device |addr|.
    static bool ConnectionParameterUpdate(const Addr& addr,
                                          int min_interval,
                                          int max_interval,
                                          int latency,
                                          int timeout);

    // Retrieve the list of services on device |addr|. They will be returned in
    // the callback |OnGetServices|.
    static bool GetServices(const Addr& addr);

    // Clear pending connect request of remote device with |addr|.
    static bool ClearPendingConnect(const Addr& addr) __attribute__((__weak__));

    // Clear pending disconnect request of remote device with |addr|.
    static bool ClearPendingDisconnect(const Addr& addr)
        __attribute__((__weak__));
  };

  // GATT Server role. Devices must implement the GATT server role in order to
  // support Cast BLE setup.
  class Server {
   public:
    class Delegate {
     public:
      // Called when a client connects or disconnects.
      virtual void OnConnectionStateChanged(const Addr& addr,
                                            bool connected) = 0;

      // Called when a service was added. |success| is false if it failed to add
      // the service.
      virtual void OnServiceAdded(bool success, const Service& service) = 0;

      // Called when a client requests a read on a characteristic.
      virtual void OnCharacteristicReadRequest(const Addr& addr,
                                               int request_id,
                                               uint16_t handle,
                                               int offset,
                                               bool is_long) = 0;

      // Called when a client requests a write on a characteristic.
      virtual void OnCharacteristicWriteRequest(
          const Addr& addr,
          int request_id,
          uint16_t handle,
          int offset,
          bool is_prepare_write,
          bool need_response,
          const std::vector<uint8_t>& value) = 0;

      // Called when a client requests a read on a descriptor.
      virtual void OnDescriptorReadRequest(const Addr& addr,
                                           int request_id,
                                           uint16_t handle,
                                           int offset,
                                           bool is_long) = 0;

      // Called when a client requests a write on a descriptor.
      virtual void OnDescriptorWriteRequest(
          const Addr& addr,
          int request_id,
          uint16_t handle,
          int offset,
          bool is_prepare_write,
          bool need_response,
          const std::vector<uint8_t>& value) = 0;

      // Called when a client performs a prepared write operation. If
      // |is_execute| is false, then it clears the currently pending prepared
      // write.
      virtual void OnExecuteWriteRequest(const Addr& addr,
                                         int request_id,
                                         bool is_execute) = 0;

      // Called when SendNotification is complete. |success| is false if it
      // failed.
      virtual void OnNotificationSent(const Addr& addr, bool success) = 0;

      virtual ~Delegate() = default;
    };

    // Returns true if this interface is implemented.
    static bool IsSupported();

    static void SetDelegate(Delegate* delegate);

    // Add |Service| to this GATT Server. On completion, OnServiceAdded will be
    // called.
    static bool AddService(const Service& service);

    // Send a response to any of the callbacks with a |request_id| above.
    static bool SendResponse(const Addr& addr,
                             int request_id,
                             Status status,
                             int offset,
                             const std::vector<uint8_t>& value);

    // Send a characteristic value notification.
    static bool SendNotification(const Addr& addr,
                                 int handle,
                                 bool confirm,
                                 const std::vector<uint8_t>& value);
  };
};

// Work around '[chromium-style] Complex class/struct needs an explicit
// out-of-line constructor.'
inline Gatt::Characteristic::Characteristic() = default;
inline Gatt::Characteristic::Characteristic(const Characteristic& other) =
    default;
inline Gatt::Characteristic::~Characteristic() = default;

inline Gatt::Service::Service() = default;
inline Gatt::Service::Service(const Service& other) = default;
inline Gatt::Service::~Service() = default;

inline bool operator==(const Gatt::Descriptor& lhs,
                       const Gatt::Descriptor& rhs) {
  return lhs.uuid == rhs.uuid && lhs.handle == rhs.handle &&
         lhs.permissions == rhs.permissions;
}

inline bool operator==(const Gatt::Characteristic& lhs,
                       const Gatt::Characteristic& rhs) {
  return lhs.uuid == rhs.uuid && lhs.handle == rhs.handle &&
         lhs.permissions == rhs.permissions &&
         lhs.properties == rhs.properties && lhs.descriptors == rhs.descriptors;
}

}  // namespace bluetooth_v2_shlib
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_BLUETOOTH_GATT_H_
