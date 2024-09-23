// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_H_

#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/sync/base/data_type.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace sync_pb {
enum SharingSpecificFields_EnabledFeatures : int;
enum SyncEnums_DeviceType : int;
enum SyncEnums_SendTabReceivingType : int;
}  // namespace sync_pb

namespace syncer {

// A class that holds information regarding the properties of a device.
class DeviceInfo {
 public:
  // A struct that holds information regarding to FCM web push.
  struct SharingTargetInfo {
    // FCM registration token of device.
    std::string fcm_token;

    // Public key for Sharing message encryption[RFC8291].
    std::string p256dh;

    // Auth secret for Sharing message encryption[RFC8291].
    std::string auth_secret;

    bool operator==(const SharingTargetInfo& other) const;
  };

  // A struct that holds information regarding to Sharing features.
  struct SharingInfo {
    SharingInfo(SharingTargetInfo vapid_target_info,
                SharingTargetInfo sharing_target_info,
                std::string chime_representative_target_id,
                std::set<sync_pb::SharingSpecificFields_EnabledFeatures>
                    enabled_features);
    SharingInfo(const SharingInfo& other);
    SharingInfo(SharingInfo&& other);
    SharingInfo& operator=(const SharingInfo& other);
    ~SharingInfo();

    // Target info using VAPID key.
    // TODO(crbug.com/40102247): Deprecate when VAPID migration is over.
    SharingTargetInfo vapid_target_info;

    // Target info using Sharing sender ID.
    SharingTargetInfo sender_id_target_info;

    // Identifier used to send messages to a specific device through Chime.
    std::string chime_representative_target_id;

    // Set of Sharing features enabled on the device.
    std::set<sync_pb::SharingSpecificFields_EnabledFeatures> enabled_features;

    bool operator==(const SharingInfo& other) const;
  };

  struct PhoneAsASecurityKeyInfo {
    // NotReady indicates that more time is needed to calculate the
    // PhoneAsASecurityKeyInfo.
    using NotReady = base::StrongAlias<class NotReadyTag, absl::monostate>;
    // NoSupport indicates that phone-as-a-security-key cannot be supported.
    using NoSupport = base::StrongAlias<class NoSupportTag, absl::monostate>;
    using StatusOrInfo =
        absl::variant<NotReady, NoSupport, PhoneAsASecurityKeyInfo>;

    PhoneAsASecurityKeyInfo();
    PhoneAsASecurityKeyInfo(const PhoneAsASecurityKeyInfo& other);
    PhoneAsASecurityKeyInfo(PhoneAsASecurityKeyInfo&& other);
    PhoneAsASecurityKeyInfo& operator=(const PhoneAsASecurityKeyInfo& other);
    ~PhoneAsASecurityKeyInfo();

    // NonRotatingFieldsEqual returns true if this object is equal to |other|,
    // ignoring the |id| and |secret| fields, which update based on the current
    // time.
    bool NonRotatingFieldsEqual(const PhoneAsASecurityKeyInfo& other) const;

    // The domain of the tunnel service. See
    // |device::cablev2::tunnelserver::DecodeDomain| to decode this value.
    uint16_t tunnel_server_domain;
    // contact_id is an opaque value that is sent to the tunnel service in order
    // to identify the caBLEv2 authenticator.
    std::vector<uint8_t> contact_id;
    // secret is the shared secret that authenticates the desktop to the
    // authenticator.
    std::array<uint8_t, 32> secret;
    // id identifies the secret so that the phone knows which secret to use
    // for a given connection.
    uint32_t id;
    // peer_public_key_x962 is the authenticator's public key.
    std::array<uint8_t, 65> peer_public_key_x962;
  };

  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.sync_device_info
  //
  enum class OsType {
    kUnknown = 0,
    kWindows = 1,
    kMac = 2,
    kLinux = 3,
    kChromeOsAsh = 4,
    kAndroid = 5,
    kIOS = 6,
    kChromeOsLacros = 7,
    kFuchsia = 8
  };

  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.sync_device_info
  //
  enum class FormFactor {
    kUnknown = 0,
    kDesktop = 1,
    kPhone = 2,
    kTablet = 3,
    kAutomotive = 4,
    kWearable = 5,
    kTv = 6,
  };

  DeviceInfo(
      const std::string& guid,
      const std::string& client_name,
      const std::string& chrome_version,
      const std::string& sync_user_agent,
      const sync_pb::SyncEnums_DeviceType device_type,
      const OsType os_type,
      const FormFactor form_factor,
      const std::string& signin_scoped_device_id,
      const std::string& manufacturer_name,
      const std::string& model_name,
      const std::string& full_hardware_class,
      base::Time last_updated_timestamp,
      base::TimeDelta pulse_interval,
      bool send_tab_to_self_receiving_enabled,
      sync_pb::SyncEnums_SendTabReceivingType send_tab_to_self_receiving_type,
      const std::optional<SharingInfo>& sharing_info,
      const std::optional<PhoneAsASecurityKeyInfo>& paask_info,
      const std::string& fcm_registration_token,
      const DataTypeSet& interested_data_types,
      std::optional<base::Time> floating_workspace_last_signin_timestamp);

  DeviceInfo(const DeviceInfo&) = delete;
  DeviceInfo& operator=(const DeviceInfo&) = delete;

  ~DeviceInfo();

  // Sync specific unique identifier for the device. Note if a device
  // is wiped and sync is set up again this id WILL be different.
  // The same device might have more than 1 guid if the device has multiple
  // accounts syncing.
  const std::string& guid() const;

  // The host name for the client.
  const std::string& client_name() const;

  // Chrome version string.
  const std::string& chrome_version() const;

  // The user agent is the combination of OS type, chrome version and which
  // channel of chrome(stable or beta). For more information see
  // |LocalDeviceInfoProviderImpl::MakeUserAgentForSyncApi|.
  const std::string& sync_user_agent() const;

  // Third party visible id for the device. See |public_id_| for more details.
  const std::string& public_id() const;

  // Device Type.
  sync_pb::SyncEnums_DeviceType device_type() const;

  // Returns the OS of this device.
  OsType os_type() const;

  // Returns the device type (form-factor).
  FormFactor form_factor() const;

  // Device_id that is stable until user signs out. This device_id is used for
  // annotating login scoped refresh token.
  const std::string& signin_scoped_device_id() const;

  // The device manufacturer name.
  const std::string& manufacturer_name() const;

  // The device model name.
  const std::string& model_name() const;

  // Returns unique hardware class string which details the
  // HW combination of a ChromeOS device. Returns empty on other OS devices or
  // when UMA is disabled.
  const std::string& full_hardware_class() const;

  // Returns the time at which this device was last updated to the sync servers.
  base::Time last_updated_timestamp() const;

  // Returns the interval with which this device is updated to the sync servers
  // if online and while sync is actively running (e.g. excludes backgrounded
  // apps on Android).
  base::TimeDelta pulse_interval() const;

  // Whether the receiving side of the SendTabToSelf feature is enabled.
  bool send_tab_to_self_receiving_enabled() const;

  // Enabled message types for the receiving side of the SendTabToSelf feature.
  // This is meaningless if send_tab_to_self_receiving_enabled() returns false.
  // If not set, the in-app message type will be assumed.
  sync_pb::SyncEnums_SendTabReceivingType send_tab_to_self_receiving_type()
      const;

  // Returns Sharing related info of the device.
  const std::optional<SharingInfo>& sharing_info() const;

  const std::optional<PhoneAsASecurityKeyInfo>& paask_info() const;

  // Returns the FCM registration token for sync invalidations.
  const std::string& fcm_registration_token() const;

  // Returns the data types for which this device receives invalidations.
  const DataTypeSet& interested_data_types() const;

  // Returns the time at which this device was last signed into the device.
  std::optional<base::Time> floating_workspace_last_signin_timestamp() const;

  // Apps can set ids for a device that is meaningful to them but
  // not unique enough so the user can be tracked. Exposing |guid|
  // would lead to a stable unique id for a device which can potentially
  // be used for tracking.
  void set_public_id(const std::string& id);

  void set_full_hardware_class(const std::string& full_hardware_class);

  void set_send_tab_to_self_receiving_enabled(bool new_value);

  void set_send_tab_to_self_receiving_type(
      sync_pb::SyncEnums_SendTabReceivingType new_value);

  void set_sharing_info(const std::optional<SharingInfo>& sharing_info);

  void set_paask_info(std::optional<PhoneAsASecurityKeyInfo>&& paask_info);

  void set_client_name(const std::string& client_name);

  void set_fcm_registration_token(const std::string& fcm_token);

  void set_interested_data_types(const DataTypeSet& data_types);

  void set_floating_workspace_last_signin_timestamp(
      std::optional<base::Time> time);

 private:
  const std::string guid_;

  std::string client_name_;

  const std::string chrome_version_;

  const std::string sync_user_agent_;

  const sync_pb::SyncEnums_DeviceType device_type_;

  const OsType os_type_;

  const FormFactor form_factor_;

  const std::string signin_scoped_device_id_;

  // Exposing |guid| would lead to a stable unique id for a device which
  // can potentially be used for tracking. Public ids are privacy safe
  // ids in that the same device will have different id for different apps
  // and they are also reset when app/extension is uninstalled.
  std::string public_id_;

  const std::string manufacturer_name_;

  const std::string model_name_;

  std::string full_hardware_class_;

  const base::Time last_updated_timestamp_;

  const base::TimeDelta pulse_interval_;

  bool send_tab_to_self_receiving_enabled_;

  sync_pb::SyncEnums_SendTabReceivingType send_tab_to_self_receiving_type_;

  std::optional<SharingInfo> sharing_info_;

  std::optional<PhoneAsASecurityKeyInfo> paask_info_;

  // An FCM registration token obtained by sync invalidations service.
  std::string fcm_registration_token_;

  // Data types for which this device receives invalidations.
  DataTypeSet interested_data_types_;

  std::optional<base::Time> floating_workspace_last_signin_timestamp_;

  // NOTE: when adding a member, don't forget to update
  // |StoredDeviceInfoStillAccurate| in device_info_sync_bridge.cc or else
  // changes in that member might not trigger uploads of updated DeviceInfos.
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_H_
