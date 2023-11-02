// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MOJO_REMOTE_INTERFACES_H_
#define CHROMECAST_MOJO_REMOTE_INTERFACES_H_

#include "base/sequence_checker.h"
#include "chromecast/mojo/mojom/remote_interfaces.mojom.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {

// Client helper object which wraps a mojo::Remote<mojom::RemoteInterfaces> and
// provides convenience methods for binding local Remote stubs.
//
// =============================================================================
// Example Usage
// =============================================================================
//
// From a service, create a pending RemoteInterfaces, such as from
// InterfaceBundle:
//
//   InterfaceBundle bundle;
//   bundle.AddInterface<mojom::Foo>(GetFooImpl());
//   bundle.AddInterface<mojom::Bar>(GetBarImpl());
//   mojo::PendingRemote<mojom::RemoteInterfaces> pending_provider =
//       bundle.CreateRemote();
//
// In a different service, we can use RemoteInterfaces to wrap
// |pending_provider| and access the interfaces that were added:
//
//   RemoteService::InjectInterfaces(
//       mojo::PendingRemote<mojom::RemoteInterfaces> pending_provider) {
//     RemoteInterfaces provider(std::move(pending_provider));
//     mojo::Remote<mojom::Bar> bar = bundle.GetRemote<mojom::Bar>();
//     bar->DoBarStuff();
//   }
class RemoteInterfaces {
 public:
  RemoteInterfaces();
  explicit RemoteInterfaces(
      mojo::PendingRemote<mojom::RemoteInterfaces> provider);
  ~RemoteInterfaces();

  RemoteInterfaces(const RemoteInterfaces&) = delete;
  RemoteInterfaces& operator=(const RemoteInterfaces&) = delete;

  // Late-binds a provider if one was not injected on creation.
  void SetProvider(mojo::PendingRemote<mojom::RemoteInterfaces> provider);

  // Exposes interfaces to a remote provider.
  mojo::PendingRemote<mojom::RemoteInterfaces> Forward();

  // Gets the currently unbound receiver to pass to a remote provider. There
  // must not already be a receiver bound to this class.
  mojo::PendingReceiver<mojom::RemoteInterfaces> GetReceiver();

  // ===========================================================================
  // Interface binding methods: After binding, the Remotes/PendingRemotes which
  // are bound are guaranteed to be bound and connected (i.e. remote.is_bound()
  // and remote.is_connected() will return true immediately). Clients can safely
  // start issuing method calls at this point. However, if the remote provider
  // does not fulfill the request, then the Remote/PendingRemote will become
  // disconnected (remote.is_connected() == false), but still be bound.
  // ===========================================================================

  // Binds a generic receiver to an implementation in the remote provider.
  void Bind(mojo::GenericPendingReceiver receiver);

  // Binds an <Interface> implementation in the remote provider.
  template <typename Interface>
  void Bind(mojo::PendingReceiver<Interface> receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    remote_provider_->BindInterface(Interface::Name_, receiver.PassPipe());
  }

  // Binds a new message pipe to |remote|, and passes the request to the remote
  // provider.
  template <typename Interface>
  void BindNewPipe(mojo::Remote<Interface>* remote) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto pending_receiver = remote->BindNewPipeAndPassReceiver();
    Bind(std::move(pending_receiver));
  }

  // Dispenses a mojo::Remote<Interface> which is bound by the remote provider.
  template <typename Interface>
  mojo::Remote<Interface> CreateRemote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    mojo::Remote<Interface> remote;
    BindNewPipe(&remote);
    return remote;
  }

 private:
  void Init();

  mojo::Remote<mojom::RemoteInterfaces> remote_provider_;

  // Temporary pending receiver which allows the client to immediately
  // start acquiring remote interfaces while we wait for an implementation
  // to be provided.
  mojo::PendingReceiver<mojom::RemoteInterfaces> waiting_receiver_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace chromecast

#endif  // CHROMECAST_MOJO_REMOTE_INTERFACES_H_
