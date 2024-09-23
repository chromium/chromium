// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_WIFI_P2P_WIFI_P2P_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_WIFI_P2P_WIFI_P2P_CONTROLLER_H_

#include "base/check.h"
#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"

namespace ash {

class WifiP2PGroup;

// Class for handling initialization and access to chromeos wifi_p2p controller.
// Exposes functions for following operations:
// 1. Create a p2p group
// 2. Destroy a p2p group
// 3. Connect to a p2p group
// 4. Disconnect from a p2p group
// 5. Fetch p2p group/client properties
// 6. Tag socket to a WiFi direct group network rules.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_WIFI_P2P) WifiP2PController
    : public ShillPropertyChangedObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnWifiDirectConnectionDisconnected(const int shill_id,
                                                    bool is_owner) = 0;
  };

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static WifiP2PController* Get();

  // Returns true if the global instance has been initialized.
  static bool IsInitialized();

  struct WifiP2PCapabilities {
    WifiP2PCapabilities(const bool is_owner_ready,
                        const bool is_client_ready,
                        const bool is_p2p_supported)
        : is_owner_ready(is_owner_ready),
          is_client_ready(is_client_ready),
          is_p2p_supported(is_p2p_supported) {}

    ~WifiP2PCapabilities() = default;

    // Whether platform is ready for creating p2p GO interface without any
    // concurrency conflict.
    bool is_owner_ready;

    // Whether platform is ready for creating p2p GC interface without any
    // concurrency conflict.
    bool is_client_ready;

    // Whether the device supports p2p operations or not.
    bool is_p2p_supported;
  };

  // Represents the Wifi P2P operation result. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class OperationResult {
    kSuccess = 0,
    // Wifi direct is disallowed in platform per Manager.P2PAllowed.
    kNotAllowed = 1,
    // Wifi direct operation is not supported in the platform.
    kNotSupported = 2,
    // Creating Wifi direct interface is not possible with existing interfaces.
    kConcurrencyNotSupported = 3,
    // The requested refruency is not supported.
    kFrequencyNotSupported = 4,
    // Wifi direct group rejects the authentication attempt.
    kAuthFailure = 5,
    // Didn't discover the Wifi direct group.
    kGroupNotFound = 6,
    // Already connected to the Wifi direct group.
    kAlreadyConnected = 7,
    // Device is not connected to a Wifi direct group.
    kNotConnected = 8,
    // Wifi direct operation is already in progress.
    kOperationInProgress = 9,
    // Invalid arguments.
    kInvalidArguments = 10,
    // Wifi direct operation timed out.
    kTimeout = 11,
    // Wifi direct operation response has an invalid result code.
    kInvalidResultCode = 12,
    // Wifi direct group miss or has invalid properties.
    kInvalidGroupProperties = 13,
    // Wifi direct operation failure.
    kOperationFailed = 14,
    // Wifi direct operation failed due to DBus error.
    kDBusError = 15,
    kMaxValue = kDBusError,
  };

  enum class OperationType {
    kCreateGroup,
    kConnectGroup,
    kDestroyGroup,
    kDisconnectGroup,
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const OperationType& type);

  // Return callback for the CreateWifiP2PGroup or ConnectToWifiP2PGroup
  // methods.
  using WifiP2PGroupCallback =
      base::OnceCallback<void(OperationResult result,
                              std::optional<WifiP2PGroup> group_metadata)>;

  // SSID and passphrase should be provided or omit at the same time. If both
  // SSID and passphrase are provide, it will attempt to create the WiFi P2P
  // group with the given `ssid` and `passphrase`. Otherwise, the platform will
  // generate the ssid and passphrase.
  void CreateWifiP2PGroup(std::optional<std::string> ssid,
                          std::optional<std::string> passphrase,
                          WifiP2PGroupCallback callback);

  // Destroys the Wifi P2P group using its shill id.
  void DestroyWifiP2PGroup(
      int shill_id,
      base::OnceCallback<void(OperationResult result)> callback);

  // Disconnects from the Wifi P2P group.
  void DisconnectFromWifiP2PGroup(
      int shill_id,
      base::OnceCallback<void(OperationResult result)> callback);

  // Connect to a Wifi P2P group with given `ssid` and `passphrase`. If
  // `frequency` is provided, the operation will fail if no group found at the
  // specified frequency. If it is omitted, the system will scan full supported
  // channels to find the group.
  void ConnectToWifiP2PGroup(const std::string& ssid,
                             const std::string& passphrase,
                             std::optional<uint32_t> frequency,
                             WifiP2PGroupCallback callback);
  const WifiP2PCapabilities& GetP2PCapabilities() const;

  // Tags the TCP/UDP socket with the given `socket_fd` to the network
  // specified by `network_id`. The `socket_fd` should be the duplicate of the
  // fd that the caller process actually keeps.
  void TagSocket(int network_id,
                 base::ScopedFD socket_fd,
                 base::OnceCallback<void(bool success)> callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  WifiP2PController();
  WifiP2PController(const WifiP2PController&) = delete;
  WifiP2PController& operator=(const WifiP2PController&) = delete;
  ~WifiP2PController() override;

  void Init();

  // ShillPropertyChangedObserver overrides
  void OnPropertyChanged(const std::string& key,
                         const base::Value& value) override;

  void OnCreateOrConnectP2PGroupSuccess(const OperationType& type,
                                        WifiP2PGroupCallback callback,
                                        base::Value::Dict result);

  void OnCreateOrConnectP2PGroupFailure(const OperationType& type,
                                        WifiP2PGroupCallback callback,
                                        const std::string& error_name,
                                        const std::string& error_message);

  void OnDestroyOrDisconnectP2PGroupSuccess(
      const OperationType& type,
      base::OnceCallback<void(OperationResult result)> callback,
      base::Value::Dict result);

  void OnDestroyOrDisconnectP2PGroupFailure(
      const OperationType& type,
      base::OnceCallback<void(OperationResult result)> callback,
      const std::string& error_name,
      const std::string& error_message);

  void OnTagSocketCompleted(base::OnceCallback<void(bool success)> callback,
                            bool success);

  void GetP2PGroupMetadata(int shill_id,
                           const OperationType& type,
                           WifiP2PGroupCallback callback,
                           std::optional<base::Value::Dict> properties);

  // Callback when set shill manager property operation failed.
  void OnSetManagerPropertyFailure(const std::string& property_name,
                                   const std::string& error_name,
                                   const std::string& error_message);

  void OnGetManagerProperties(std::optional<base::Value::Dict> properties);

  void UpdateP2PCapabilities(const base::Value::Dict& capabilities);

  void CompleteWifiP2PGroupCallback(const OperationType& type,
                                    const OperationResult& result,
                                    WifiP2PGroupCallback callback,
                                    std::optional<WifiP2PGroup> group_metadata);

  void CheckAndNotifyDisconnection(bool is_owner,
                                   const base::Value& property_list,
                                   const std::string& interface_state_property,
                                   const std::string& shill_id_property,
                                   const std::string& idle_state_property);

  base::ObserverList<Observer> observer_list_;

  WifiP2PCapabilities wifi_p2p_capabilities_{false, false, false};

  base::WeakPtrFactory<WifiP2PController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_WIFI_P2P_WIFI_P2P_CONTROLLER_H_
