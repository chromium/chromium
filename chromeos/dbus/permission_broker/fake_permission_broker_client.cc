// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/permission_broker/fake_permission_broker_client.h"

#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/unguessable_token.h"

namespace chromeos {

namespace {

constexpr char kOpenFailedError[] = "open_failed";
constexpr char kDupFailedError[] = "dup_failed";
constexpr char kWatchLifelineFdFailedError[] = "watch_lifeline_fd_failed";

FakePermissionBrokerClient* g_instance = nullptr;

// So that real devices can be accessed by tests and "Chromium OS on Linux" this
// function implements a simplified version of the method implemented by the
// permission broker by opening the path specified and returning the resulting
// file descriptor.
void OpenPath(const std::string& path,
              PermissionBrokerClient::OpenPathCallback callback,
              PermissionBrokerClient::ErrorCallback error_callback,
              scoped_refptr<base::TaskRunner> task_runner) {
  base::ScopedFD fd(HANDLE_EINTR(open(path.c_str(), O_RDWR)));
  if (!fd.is_valid()) {
    int error_code = logging::GetLastSystemErrorCode();
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(error_callback), kOpenFailedError,
            base::StringPrintf(
                "Failed to open '%s': %s", path.c_str(),
                logging::SystemErrorCodeToString(error_code).c_str())));
    return;
  }

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), std::move(fd)));
}

bool DisconnectInterface(const std::string& path, uint8_t iface_num) {
  base::ScopedFD fd(HANDLE_EINTR(open(path.c_str(), O_RDWR)));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to open path " << path;
    return false;
  }

  struct usbdevfs_ioctl dio = {};
  dio.ifno = iface_num;
  dio.ioctl_code = USBDEVFS_DISCONNECT;
  dio.data = nullptr;
  int rc = HANDLE_EINTR(ioctl(fd.get(), USBDEVFS_IOCTL, &dio));
  if (rc < 0) {
    PLOG(ERROR) << "Failed to disconnect interface "
                << static_cast<int>(iface_num) << " on path " << path;
    return false;
  }
  return true;
}

bool ConnectInterface(const std::string& path, uint8_t iface_num) {
  base::ScopedFD fd(HANDLE_EINTR(open(path.c_str(), O_RDWR)));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to open path " << path;
    return false;
  }

  struct usbdevfs_ioctl dio = {};
  dio.ifno = iface_num;
  dio.ioctl_code = USBDEVFS_CONNECT;
  dio.data = nullptr;
  int rc = HANDLE_EINTR(ioctl(fd.get(), USBDEVFS_IOCTL, &dio));
  if (rc < 0) {
    PLOG(ERROR) << "Failed to connect interface " << static_cast<int>(iface_num)
                << " on path " << path;
    return false;
  }
  return true;
}

}  // namespace

FakePermissionBrokerClient::FakePermissionBrokerClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakePermissionBrokerClient::~FakePermissionBrokerClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakePermissionBrokerClient* FakePermissionBrokerClient::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void FakePermissionBrokerClient::CheckPathAccess(const std::string& path,
                                                 ResultCallback callback) {
  std::move(callback).Run(true);
}

void FakePermissionBrokerClient::OpenPath(const std::string& path,
                                          OpenPathCallback callback,
                                          ErrorCallback error_callback) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&chromeos::OpenPath, path, std::move(callback),
                     std::move(error_callback),
                     base::SingleThreadTaskRunner::GetCurrentDefault()));
}

void FakePermissionBrokerClient::ClaimDevicePath(
    const std::string& path,
    uint32_t allowed_interfaces_mask,
    int lifeline_fd,
    OpenPathCallback callback,
    ErrorCallback error_callback) {
  OpenPath(path, std::move(callback), std::move(error_callback));
}

void FakePermissionBrokerClient::OpenPathAndRegisterClient(
    const std::string& path,
    uint32_t allowed_interfaces_mask,
    int lifeline_fd,
    OpenPathAndRegisterClientCallback callback,
    ErrorCallback error_callback) {
  std::string client_id;
  do {
    client_id = base::UnguessableToken::Create().ToString();
  } while (base::Contains(clients_, client_id));

  base::ScopedFD dup_lifeline_fd(HANDLE_EINTR(dup(lifeline_fd)));
  if (!dup_lifeline_fd.is_valid()) {
    int error_code = logging::GetLastSystemErrorCode();
    std::move(error_callback)
        .Run(kDupFailedError,
             base::StringPrintf(
                 "Failed to dup lifeline fd %d: %s", lifeline_fd,
                 logging::SystemErrorCodeToString(error_code).c_str()));
    return;
  }

  auto controller = base::FileDescriptorWatcher::WatchReadable(
      dup_lifeline_fd.get(),
      base::BindRepeating(&FakePermissionBrokerClient::HandleClosedClient,
                          weak_factory_.GetWeakPtr(), client_id));
  if (!controller) {
    std::move(error_callback)
        .Run(kWatchLifelineFdFailedError,
             base::StringPrintf("Failed to watch dup lifeline fd %d",
                                dup_lifeline_fd.get()));
    return;
  }
  clients_.emplace(client_id, UsbInterfaces(path, std::move(dup_lifeline_fd),
                                            std::move(controller)));

  // No concern of OpenPath failure causing orphan client record here, as the
  // inserted client's record will still be removed when requester does error
  // handling and closes the lifeline_fd.
  OpenPath(path, base::BindOnce(std::move(callback), client_id),
           std::move(error_callback));
}

void FakePermissionBrokerClient::DetachInterface(const std::string& client_id,
                                                 uint8_t iface_num,
                                                 ResultCallback callback) {
  auto client_it = clients_.find(client_id);
  if (client_it == clients_.end()) {
    LOG(ERROR) << "Unknown client_id: " << client_id;
    std::move(callback).Run(false);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&chromeos::DisconnectInterface, client_it->second.path,
                     iface_num),
      std::move(callback));
}

void FakePermissionBrokerClient::ReattachInterface(const std::string& client_id,
                                                   uint8_t iface_num,
                                                   ResultCallback callback) {
  auto client_it = clients_.find(client_id);
  if (client_it == clients_.end()) {
    LOG(ERROR) << "Unknown client_id: " << client_id;
    std::move(callback).Run(false);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&chromeos::ConnectInterface, client_it->second.path,
                     iface_num),
      std::move(callback));
}

void FakePermissionBrokerClient::RequestTcpPortAccess(
    uint16_t port,
    const std::string& interface,
    int lifeline_fd,
    ResultCallback callback) {
  if (tcp_deny_all_) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(
      RequestPortImpl(port, interface, tcp_deny_rule_set_, &tcp_hole_set_));
}

void FakePermissionBrokerClient::RequestUdpPortAccess(
    uint16_t port,
    const std::string& interface,
    int lifeline_fd,
    ResultCallback callback) {
  if (udp_deny_all_) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(
      RequestPortImpl(port, interface, udp_deny_rule_set_, &udp_hole_set_));
}

void FakePermissionBrokerClient::ReleaseTcpPort(uint16_t port,
                                                const std::string& interface,
                                                ResultCallback callback) {
  std::move(callback).Run(tcp_hole_set_.erase(std::make_pair(port, interface)));
  if (delegate_) {
    delegate_->OnTcpPortReleased(port, interface);
  }
}

void FakePermissionBrokerClient::ReleaseUdpPort(uint16_t port,
                                                const std::string& interface,
                                                ResultCallback callback) {
  std::move(callback).Run(udp_hole_set_.erase(std::make_pair(port, interface)));
  if (delegate_) {
    delegate_->OnUdpPortReleased(port, interface);
  }
}

void FakePermissionBrokerClient::AddTcpDenyRule(uint16_t port,
                                                const std::string& interface) {
  tcp_deny_rule_set_.insert(std::make_pair(port, interface));
}

void FakePermissionBrokerClient::SetTcpDenyAll() {
  tcp_deny_all_ = true;
}

void FakePermissionBrokerClient::AddUdpDenyRule(uint16_t port,
                                                const std::string& interface) {
  udp_deny_rule_set_.insert(std::make_pair(port, interface));
}

void FakePermissionBrokerClient::SetUdpDenyAll() {
  udp_deny_all_ = true;
}

bool FakePermissionBrokerClient::HasTcpHole(uint16_t port,
                                            const std::string& interface) {
  auto rule = std::make_pair(port, interface);
  return tcp_hole_set_.find(rule) != tcp_hole_set_.end();
}

bool FakePermissionBrokerClient::HasUdpHole(uint16_t port,
                                            const std::string& interface) {
  auto rule = std::make_pair(port, interface);
  return udp_hole_set_.find(rule) != udp_hole_set_.end();
}

bool FakePermissionBrokerClient::HasTcpPortForward(
    uint16_t port,
    const std::string& interface) {
  auto rule = std::make_pair(port, interface);
  return tcp_forwarding_set_.find(rule) != tcp_forwarding_set_.end();
}

bool FakePermissionBrokerClient::HasUdpPortForward(
    uint16_t port,
    const std::string& interface) {
  auto rule = std::make_pair(port, interface);
  return udp_forwarding_set_.find(rule) != udp_forwarding_set_.end();
}

void FakePermissionBrokerClient::RequestTcpPortForward(
    uint16_t in_port,
    const std::string& in_interface,
    const std::string& dst_ip,
    uint16_t dst_port,
    int lifeline_fd,
    ResultCallback callback) {
  // TODO(matterchen): Increase logic for adding duplicate ports.
  auto rule = std::make_pair(in_port, in_interface);
  tcp_forwarding_set_.insert(rule);
  std::move(callback).Run(true);
}

void FakePermissionBrokerClient::RequestUdpPortForward(
    uint16_t in_port,
    const std::string& in_interface,
    const std::string& dst_ip,
    uint16_t dst_port,
    int lifeline_fd,
    ResultCallback callback) {
  auto rule = std::make_pair(in_port, in_interface);
  udp_forwarding_set_.insert(rule);
  std::move(callback).Run(true);
}

void FakePermissionBrokerClient::ReleaseTcpPortForward(
    uint16_t in_port,
    const std::string& in_interface,
    ResultCallback callback) {
  auto rule = std::make_pair(in_port, in_interface);
  tcp_forwarding_set_.erase(rule);
  std::move(callback).Run(true);
}

void FakePermissionBrokerClient::ReleaseUdpPortForward(
    uint16_t in_port,
    const std::string& in_interface,
    ResultCallback callback) {
  auto rule = std::make_pair(in_port, in_interface);
  udp_forwarding_set_.erase(rule);
  std::move(callback).Run(true);
}

void FakePermissionBrokerClient::AttachDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

bool FakePermissionBrokerClient::RequestPortImpl(uint16_t port,
                                                 const std::string& interface,
                                                 const RuleSet& deny_rule_set,
                                                 RuleSet* hole_set) {
  auto rule = std::make_pair(port, interface);

  // If there is already a hole, returns true.
  if (hole_set->find(rule) != hole_set->end())
    return true;

  // If it is denied to make a hole, returns false.
  if (deny_rule_set.find(rule) != deny_rule_set.end())
    return false;

  hole_set->insert(rule);
  return true;
}

FakePermissionBrokerClient::UsbInterfaces::UsbInterfaces(
    const std::string& path,
    base::ScopedFD lifeline_fd,
    std::unique_ptr<base::FileDescriptorWatcher::Controller> controller)
    : path(std::move(path)),
      lifeline_fd(std::move(lifeline_fd)),
      controller(std::move(controller)) {}

FakePermissionBrokerClient::UsbInterfaces::~UsbInterfaces() = default;

FakePermissionBrokerClient::UsbInterfaces::UsbInterfaces(UsbInterfaces&&) =
    default;
FakePermissionBrokerClient::UsbInterfaces&
FakePermissionBrokerClient::UsbInterfaces::operator=(UsbInterfaces&&) = default;

}  // namespace chromeos
