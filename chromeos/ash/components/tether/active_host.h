// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_ACTIVE_HOST_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_ACTIVE_HOST_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

namespace tether {

class TetherHostFetcher;

// ActiveHost tracks metadata about the current connection to a tether host.
// This data is persisted to user preferences.
class ActiveHost {
 public:
  // Enumeration used to describe the state of the active connection to a tether
  // host.
  enum class ActiveHostStatus {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2
  };
  static std::string StatusToString(const ActiveHostStatus& status);

  struct ActiveHostChangeInfo {
    ActiveHostChangeInfo();
    ActiveHostChangeInfo(
        ActiveHostStatus new_status,
        ActiveHostStatus old_status,
        std::optional<multidevice::RemoteDeviceRef> new_active_host,
        std::string old_active_host_id,
        std::string new_tether_network_guid,
        std::string old_tether_network_guid,
        std::string new_wifi_network_guid,
        std::string old_wifi_network_guid);
    ActiveHostChangeInfo(const ActiveHostChangeInfo& other);

    virtual ~ActiveHostChangeInfo();

    friend bool operator==(const ActiveHostChangeInfo& first,
                           const ActiveHostChangeInfo& second);

    ActiveHostStatus new_status;
    ActiveHostStatus old_status;

    // |new_active_host| will be empty if |new_status| is DISCONNECTED.
    std::optional<multidevice::RemoteDeviceRef> new_active_host;
    // |old_active_host_id| will be "" if |old_status| is DISCONNECTED.
    std::string old_active_host_id;

    // |new_tether_network_guid| will be "" if |new_status| is DISCONNECTED.
    std::string new_tether_network_guid;
    // |old_tether_network_guid| will be "" if |old_status| is DISCONNECTED.
    std::string old_tether_network_guid;

    // |new_wifi_network_guid| will be "" if |new_status| is not CONNECTED.
    std::string new_wifi_network_guid;
    // |old_wifi_network_guid| will be "" if |old_status| is not CONNECTED.
    std::string old_wifi_network_guid;
  };

  class Observer {
   public:
    virtual void OnActiveHostChanged(
        const ActiveHostChangeInfo& active_host_change_info) = 0;
  };

  ActiveHost(TetherHostFetcher* tether_host_fetcher, PrefService* pref_service);

  ActiveHost(const ActiveHost&) = delete;
  ActiveHost& operator=(const ActiveHost&) = delete;

  virtual ~ActiveHost();

  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Sets the active host to be no host at all (i.e., the local device is not
  // connecting or connected to a tether host).
  virtual void SetActiveHostDisconnected();

  // Sets the active host to be the device with ID |active_host_device_id| and
  // the associated tether network GUID to |tether_network_guid| and records
  // that the there is an active attempt to connect to that host (i.e., the host
  // is not yet connected but it is in the process of connecting).
  virtual void SetActiveHostConnecting(const std::string& active_host_device_id,
                                       const std::string& tether_network_guid);

  // Sets the active host to be the device with ID |active_host_device_id| and
  // that the local device is connected to that device on the mobile hotspot
  // with tether network GUID |tether_network_guid| and Wi-Fi network GUID
  // |wifi_network_guid|.
  virtual void SetActiveHostConnected(const std::string& active_host_device_id,
                                      const std::string& tether_network_guid,
                                      const std::string& wifi_network_guid);

  // Gets the active host and associated metadata asynchronously. If
  // the active host status is...
  //     DISCONNECTED: The callback's |active_host| parameter will be nullptr
  //                   and |wifi_network_guid| and |tether_network_guid|
  //                   parameters will be "".
  //     CONNECTING: The callback's |wifi_network_guid| parameter will be "".
  //     CONNECTED: All four parameters  will be present.
  using ActiveHostCallback = base::OnceCallback<void(
      ActiveHostStatus active_host_status,
      std::optional<multidevice::RemoteDeviceRef> active_host,
      const std::string& tether_network_guid,
      const std::string& wifi_network_guid)>;
  virtual void GetActiveHost(ActiveHostCallback active_host_callback);

  // Synchronous getter methods which do not return a full RemoteDevice object.
  virtual ActiveHostStatus GetActiveHostStatus() const;
  virtual std::string GetActiveHostDeviceId() const;
  virtual std::string GetWifiNetworkGuid() const;
  virtual std::string GetTetherNetworkGuid() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void SendActiveHostChangedUpdate(
      ActiveHostStatus old_status,
      const std::string& old_active_host_id,
      const std::string& old_tether_network_guid,
      const std::string& old_wifi_network_guid,
      ActiveHostStatus new_status,
      std::optional<multidevice::RemoteDeviceRef> new_active_host,
      const std::string& new_tether_network_guid,
      const std::string& new_wifi_network_guid);

 private:
  friend class CrashRecoveryManagerImpl;

  void SetActiveHost(ActiveHostStatus active_host_status,
                     const std::string& active_host_device_id,
                     const std::string& tether_network_guid,
                     const std::string& wifi_network_guid);

  raw_ptr<TetherHostFetcher> tether_host_fetcher_;
  raw_ptr<PrefService> pref_service_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<ActiveHost> weak_ptr_factory_{this};
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_ACTIVE_HOST_H_
