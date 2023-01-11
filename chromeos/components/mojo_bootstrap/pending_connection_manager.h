// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MOJO_BOOTSTRAP_PENDING_CONNECTION_MANAGER_H_
#define CHROMEOS_COMPONENTS_MOJO_BOOTSTRAP_PENDING_CONNECTION_MANAGER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"

namespace mojo_bootstrap {

class PendingConnectionManagerTest;

// PendingConnectionManager is used to wait for a unix domain socket to be used
// for bootstrapping a Mojo connection. The typical use case is where a system
// service is started by Chrome, but only text arguments can be passed to the
// service startup. An example of this are filesystems mounted by cros-disks,
// where only an array of strings can be passed as options to the Mount() D-Bus
// method. This class is NOT needed if a socket end can be passed to the system
// service directly.
//
// To use this class:
// 1. Create and export a D-Bus service in Chrome containing a single method
//    which is passed a string and FD. (eg. org.chromium.DriveFileStream).
//    This method simply calls PendingConnectionManager::OpenIpcChannel().
// 2. Create an UnguessableToken and pass the string serialisation to the system
//    service. Use PendingConnectionManager::ExpectOpenIpcChannel() to wait for
//    an FD.
// 3. In the system service, create a socketpair() and pass one end over the
//    D-Bus method created in step 1, along with the token.
//
// Sample usage:
// --- Chrome ---
// class MyClass {
//  private:
//   const base::UnguessableToken token_;
//   mojo::OutgoingInvitation invitation_;
// };
//
// MyClass::MyClass() : token_(base::UnguessableToken::Create()) {}
//
// void MyClass::StartService() {
//   // Use this message pipe to bind an InterfacePtr<> to the Mojo service.
//   mojo::ScopedMessagePipeHandle bootstrap_handle =
//       invitation_.AttachMessagePipe("myservice-bootstrap");
//
//   base::UnguessableToken token = base::UnguessableToken::Create();
//   PendingConnectionManager::Get().ExpectOpenIpcChannel(
//       token_, base::BindOnce(&MyClass::AcceptConnection,
//                              base::Unretained(this)));
//   StartMySystemService(token_.ToString());
// }
//
// void MyClass::AcceptConnection(base::ScopedFD handle) {
//   mojo::OutgoingInvitation::Send(
//       std::move(invitation_), base::kNullProcessHandle,
//       mojo::PlatformChannelEndpoint(
//           mojo::PlatformHandle(std::move(handle))));
// }
//
// void MyClass::MyExportedDbusConnectMethod(
//     dbus::MethodCall* method_call,
//     dbus::ExportedObject::ResponseSender response_sender) {
//   std::string token = // Pop token passed to StartMySystemService()
//   base::ScopedFD fd = // Pop FD
//   CHECK(PendingConnectionManager::Get().OpenIpcChannel(
//       token, std::move(fd)));
// }
//
// --- System Service ---
// // Returns an InterfaceRequest<> that can be used to bind the Mojo service
// // implementation.
// mojom::MyServiceRequest MyService::BootstrapMojo() {
//   mojo::edk::PlatformChannelPair channel;
//   org::chromium::MyChromeServiceProxy dbus_proxy(bus_, kServiceName);
//   brillo::ErrorPtr error;
//   CHECK(dbus_proxy.MyExportedDbusConnectMethod(
//       token_, channel.PassClientHandle().get().handle, &error));
//   mojo::edk::SetParentPipeHandle(channel.PassServerHandle());
//
//   mojom::MyServiceRequest request;
//   request.Bind(mojo::edk::CreateChildMessagePipe("myservice-bootstrap"));
//   return request;
// }
class COMPONENT_EXPORT(MOJO_BOOTSTRAP) PendingConnectionManager {
 public:
  using OpenIpcChannelCallback = base::OnceCallback<void(base::ScopedFD)>;

  static PendingConnectionManager& Get();

  PendingConnectionManager(const PendingConnectionManager&) = delete;
  PendingConnectionManager& operator=(const PendingConnectionManager&) = delete;

  // Responds to a file descriptor request for |token| with |fd|. |token| is the
  // UnguessableToken::ToString() representation of the |token| parameter to
  // ExpectOpenIpcChannel().
  bool OpenIpcChannel(const std::string& token, base::ScopedFD ipc_channel);

  // Registers a callback that is run when a file descriptor is received for
  // |token|.
  void ExpectOpenIpcChannel(base::UnguessableToken token,
                            OpenIpcChannelCallback handler);

  // Cancels the pending callback for |token|.
  void CancelExpectedOpenIpcChannel(base::UnguessableToken token);

 private:
  friend class base::NoDestructor<PendingConnectionManager>;
  friend class PendingConnectionManagerTest;

  PendingConnectionManager();
  ~PendingConnectionManager();

  base::flat_map<std::string, OpenIpcChannelCallback>
      open_ipc_channel_callbacks_;
};

}  // namespace mojo_bootstrap

#endif  // CHROMEOS_COMPONENTS_MOJO_BOOTSTRAP_PENDING_CONNECTION_MANAGER_H_
