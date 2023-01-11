// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MOJO_INTERFACE_BUNDLE_H_
#define CHROMECAST_MOJO_INTERFACE_BUNDLE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/mojo/binder_factory.h"
#include "chromecast/mojo/mojom/remote_interfaces.mojom.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {

// This class essentially combines RemoteInterfaces and BinderRegistry into
// one. On the host side, this class can be created and used like a
// BinderRegistry. InterfaceBundle can also dispense RemoteInterfaces remotes
// for clients to invoke local binders.
//
// This class should only be used on one sequence. Local interfaces will be
// bound to the sequence that the InterfaceBundle is created on, unless a task
// runner is provided in AddBinder().
//
// Implementations that are added via raw pointer (instead of a binder callback)
// will have the Receiver owned by the InterfaceBundle. When the InterfaceBundle
// is destroyed, the connection is severed. All implementations that are added
// by raw pointer must therefore outlive the InterfaceBundle.
//
// =============================================================================
// Example Usage
// =============================================================================
//
// Add implementations to the bundle, no Binding boilerplate required:
//
//   InterfaceBundle bundle;
//   bundle.AddInterface<mojom::Foo>(GetFooImpl());
//   bundle.AddInterface<mojom::Bar>(GetBarImpl());
//
// Dispense a RemoteInterfaces, which can be used by clients:
//
//   mojo::Remote<mojom::RemoteInterfaces> provider(bundle.CreateRemote());
//   mojo::Remote<mojom::Bar> bar;
//   provider->BindNewPipe(&bar);
//   bar->DoBarStuff();
class InterfaceBundle final : private mojom::RemoteInterfaces {
 public:
  // Specifies the number of expected clients for a given Receiver.
  enum ReceiverType {
    MULTIPLE_BINDERS,  // Multiple clients, use a ReceiverSet.
    SINGLE_BINDER,     // Single client, use a Receiver.
  };

  InterfaceBundle();
  InterfaceBundle(const InterfaceBundle&) = delete;
  ~InterfaceBundle() override;
  InterfaceBundle& operator=(const InterfaceBundle&) = delete;

  // Adds an implementation for an interface of type <Interface>. When the
  // interface is requested via one of the consumer methods below, |interface|
  // will receive the method calls.
  //
  // |interface| *must* outlive the InterfaceBundle, or else mojo method calls
  // could be invoked on a destroyed object.
  template <typename Interface>
  bool AddInterface(Interface* interface,
                    ReceiverType receiver_type = MULTIPLE_BINDERS) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (local_interfaces_.HasInterface<Interface>()) {
      LOG(DFATAL) << "Local interface '" << Interface::Name_ << "' has already "
                  << "been added to this bundle.";
      return false;
    }
    if (receiver_type == MULTIPLE_BINDERS) {
      local_interfaces_.AddInterface<Interface>(interface);
    } else {
      local_interfaces_.AddSingleBinderInterface<Interface>(interface);
    }
    return true;
  }

  // Similar to BinderRegistry::AddInterface(), AddBinder() allows clients to
  // provide their own binder callbacks and task runners. If |task_runner| is
  // provided, then |callback| will be invoked on |task_runner| for every
  // incoming request to <Interface>. Subsequent calls to <Interface>'s methods
  // will post to |task_runner|. If |task_runner| is not provided, the current
  // SequencedTaskRunner will receive incoming bind requests and method calls.
  template <typename Interface>
  bool AddBinder(
      const base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)>&
          callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (local_interfaces_.HasInterface<Interface>()) {
      LOG(DFATAL) << "Local interface '" << Interface::Name_ << "' has already "
                  << "been added to this bundle.";
      return false;
    }
    local_interfaces_.AddBinder<Interface>(callback, task_runner);
    return true;
  }

  template <typename Interface>
  void RemoveInterface() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    local_interfaces_.RemoveInterface<Interface>();
  }

  // Creates a remote reference which can be passed over IPC to a remote client.
  mojo::PendingRemote<mojom::RemoteInterfaces> CreateRemote();

  // Severs all client connections. This should only be called on teardown.
  void Close();

  // mojom::RemoteInterfaces implementation:
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle handle) override;
  void AddClient(
      mojo::PendingReceiver<mojom::RemoteInterfaces> receiver) override;

  // Attempt to bind a generic receiver. Succeeds if there is an available
  // implementation or binder callback registered.
  bool TryBindReceiver(mojo::GenericPendingReceiver& receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (local_interfaces_.HasInterface(*receiver.interface_name())) {
      std::string interface_name = *receiver.interface_name();
      local_interfaces_.Bind(interface_name, receiver.PassPipe());
      return true;
    }
    return false;
  }

 private:
  // For interfaces that are provided as a local pointer without any binding
  // logic, we can use MultiBinderFactory to expose a binding surface.
  MultiBinderFactory local_interfaces_;

  mojo::ReceiverSet<mojom::RemoteInterfaces> client_receivers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace chromecast

#endif  // CHROMECAST_MOJO_INTERFACE_BUNDLE_H_
