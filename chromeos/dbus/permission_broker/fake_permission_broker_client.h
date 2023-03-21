// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_PERMISSION_BROKER_FAKE_PERMISSION_BROKER_CLIENT_H_
#define CHROMEOS_DBUS_PERMISSION_BROKER_FAKE_PERMISSION_BROKER_CLIENT_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"

namespace chromeos {

class COMPONENT_EXPORT(PERMISSION_BROKER) FakePermissionBrokerClient
    : public PermissionBrokerClient {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnTcpPortReleased(uint16_t port,
                                   const std::string& interface) {}
    virtual void OnUdpPortReleased(uint16_t port,
                                   const std::string& interface) {}
  };

  FakePermissionBrokerClient();

  FakePermissionBrokerClient(const FakePermissionBrokerClient&) = delete;
  FakePermissionBrokerClient& operator=(const FakePermissionBrokerClient&) =
      delete;

  ~FakePermissionBrokerClient() override;

  // Checks that a fake instance was initialized and returns it.
  static FakePermissionBrokerClient* Get();

  void CheckPathAccess(const std::string& path,
                       ResultCallback callback) override;
  void OpenPath(const std::string& path,
                OpenPathCallback callback,
                ErrorCallback error_callback) override;
  void ClaimDevicePath(const std::string& path,
                       uint32_t allowed_interfaces_mask,
                       int lifeline_fd,
                       OpenPathCallback callback,
                       ErrorCallback error_callback) override;
  void OpenPathAndRegisterClient(const std::string& path,
                                 uint32_t allowed_interfaces_mask,
                                 int lifeline_fd,
                                 OpenPathAndRegisterClientCallback callback,
                                 ErrorCallback error_callback) override;
  void DetachInterface(const std::string& client_id,
                       uint8_t iface_num,
                       ResultCallback callback) override;
  void ReattachInterface(const std::string& client_id,
                         uint8_t iface_num,
                         ResultCallback callback) override;
  void RequestTcpPortAccess(uint16_t port,
                            const std::string& interface,
                            int lifeline_fd,
                            ResultCallback callback) override;
  void RequestUdpPortAccess(uint16_t port,
                            const std::string& interface,
                            int lifeline_fd,
                            ResultCallback callback) override;
  void ReleaseTcpPort(uint16_t port,
                      const std::string& interface,
                      ResultCallback callback) override;
  void ReleaseUdpPort(uint16_t port,
                      const std::string& interface,
                      ResultCallback callback) override;
  void RequestTcpPortForward(uint16_t in_port,
                             const std::string& in_interface,
                             const std::string& dst_ip,
                             uint16_t dst_port,
                             int lifeline_fd,
                             ResultCallback callback) override;
  void RequestUdpPortForward(uint16_t in_port,
                             const std::string& in_interface,
                             const std::string& dst_ip,
                             uint16_t dst_port,
                             int lifeline_fd,
                             ResultCallback callback) override;
  void ReleaseTcpPortForward(uint16_t in_port,
                             const std::string& in_interface,
                             ResultCallback callback) override;
  void ReleaseUdpPortForward(uint16_t in_port,
                             const std::string& in_interface,
                             ResultCallback callback) override;

  // Add a rule to have RequestTcpPortAccess fail.
  void AddTcpDenyRule(uint16_t port, const std::string& interface);

  // Unconditionally fail all RequestTcpPortAccess calls.
  void SetTcpDenyAll();

  // Add a rule to have RequestUdpPortAccess fail.
  void AddUdpDenyRule(uint16_t port, const std::string& interface);

  // Unconditionally fail all RequestUdpPortAccess calls.
  void SetUdpDenyAll();

  // Returns true if TCP port has a hole.
  bool HasTcpHole(uint16_t port, const std::string& interface);

  // Returns true if UDP port has a hole.
  bool HasUdpHole(uint16_t port, const std::string& interface);

  // Returns true if TCP port is being forwarded.
  bool HasTcpPortForward(uint16_t port, const std::string& interface);

  // Returns true if UDP port is being forwarded.
  bool HasUdpPortForward(uint16_t port, const std::string& interface);

  void AttachDelegate(Delegate* delegate);

 private:
  using RuleSet =
      std::set<std::pair<uint16_t /* port */, std::string /* interface */>>;

  struct UsbInterfaces {
    UsbInterfaces(
        const std::string& path,
        base::ScopedFD lifeline_fd,
        std::unique_ptr<base::FileDescriptorWatcher::Controller> controller);
    ~UsbInterfaces();
    UsbInterfaces(UsbInterfaces&&);
    UsbInterfaces& operator=(UsbInterfaces&&);

    UsbInterfaces(const UsbInterfaces&) = delete;
    UsbInterfaces& operator=(const UsbInterfaces&) = delete;

    std::string path;

    // NOTE: The ordering of these fields is important: `controller` must be
    // destroyed before `lifetime_fd` is closed.
    base::ScopedFD lifeline_fd;
    std::unique_ptr<base::FileDescriptorWatcher::Controller> controller;
  };

  bool RequestPortImpl(uint16_t port,
                       const std::string& interface,
                       const RuleSet& deny_rule_set,
                       RuleSet* hole_set);

  void HandleClosedClient(const std::string& client_id) {
    clients_.erase(client_id);
  }

  RuleSet tcp_hole_set_;
  RuleSet udp_hole_set_;

  RuleSet tcp_forwarding_set_;
  RuleSet udp_forwarding_set_;

  RuleSet tcp_deny_rule_set_;
  RuleSet udp_deny_rule_set_;

  bool tcp_deny_all_ = false;
  bool udp_deny_all_ = false;

  std::map<std::string, UsbInterfaces> clients_;

  raw_ptr<Delegate, DanglingUntriaged> delegate_ = nullptr;

  base::WeakPtrFactory<FakePermissionBrokerClient> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_PERMISSION_BROKER_FAKE_PERMISSION_BROKER_CLIENT_H_
