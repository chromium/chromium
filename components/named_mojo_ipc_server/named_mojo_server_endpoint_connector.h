// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_SERVER_ENDPOINT_CONNECTOR_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_SERVER_ENDPOINT_CONNECTOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "build/buildflag.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace named_mojo_ipc_server {

struct ConnectionInfo;

// Interface to allow platform-specific implementations to establish connection
// between the server endpoint and the client. mojo::IsolatedConnection can
// take a PlatformChannelServerEndpoint directly, but our implementations allow:
// 1. Reliably knowing when a new invitation needs to be sent; with the
//    alternative approach, the best we could do is to wait for an incoming IPC
//    call, which isn't reliable since a (malicious) client may clog the channel
//    by connecting and hanging without making any IPCs.
// 2. Observing the client process' PID without passing it via IPC, which
//    wouldn't be feasible with the alternative approach, since mojo doesn't
//    expose the underlying socket/named pipe.
class NamedMojoServerEndpointConnector {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when a client has connected to the server endpoint.
    virtual void OnClientConnected(
        mojo::PlatformChannelEndpoint client_endpoint,
        std::unique_ptr<ConnectionInfo> info) = 0;

    // Called to notify unittests that the server endpoint has been created.
    virtual void OnServerEndpointCreated() = 0;

   protected:
    Delegate() = default;
  };

  virtual ~NamedMojoServerEndpointConnector();

  // Configures this object to begin listening for and connecting endpoints.
  // Implementations will attempt to acquire platform-specific handles, retrying
  // indefinitely until acquired or this object is destroyed.
  void Start();

  // Creates the platform-specific MojoServerEndpointConnector.
  // |delegate| must outlive the created object. The endpoint connector will be
  // bound to |io_sequence| and post replies to current sequence.
  static base::SequenceBound<NamedMojoServerEndpointConnector> Create(
      scoped_refptr<base::SequencedTaskRunner> io_sequence,
      const EndpointOptions& options,
      base::SequenceBound<Delegate> delegate);

 protected:
  NamedMojoServerEndpointConnector(const EndpointOptions& options,
                                   base::SequenceBound<Delegate> delegate);

  // If this method returns false, it will be called again with a delay; a
  // subclass is free is always return true and handle reconnect itself.
  virtual bool TryStart() = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  const EndpointOptions options_;
  base::SequenceBound<Delegate> delegate_;

  base::WeakPtrFactory<NamedMojoServerEndpointConnector> weak_ptr_factory_{
      this};
};

}  // namespace named_mojo_ipc_server

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_SERVER_ENDPOINT_CONNECTOR_H_
