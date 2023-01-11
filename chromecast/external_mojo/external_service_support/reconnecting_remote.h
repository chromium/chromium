// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_RECONNECTING_REMOTE_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_RECONNECTING_REMOTE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {

// A class which wraps a mojo::Remote with automatic reconnection logic.
//
// Two reconnection methods are supported: (1) Provide a service name and an
// ExternalConnector to reconnect, or (2) provide a callback to rebind the
// remote.
//
// Clients can register observer callbacks to be notified of reconnect events so
// that they can re-initialize some state in the remote process. Observers are
// notified in the same order they were registered. Observers should use WeakPtr
// if they expect to outlive the ReconnectingRemote.
//
// This class can also be used to wrap a local implementation. This can be used
// for (1) Client code which can exist in both in and out-of-process, and (2)
// Injecting a mock implementation. Since the impl is called directly, this
// allows for synchronous method call validation, as opposed to asynchronously
// posting mojo calls which require a base::RunLoop to verify in unit tests.
//
template <typename Interface>
class ReconnectingRemote {
 public:
  // Reconnect option 1: Provide an ExternalConnector to request the interface
  // from a named service.
  ReconnectingRemote(const std::string& service_name,
                     external_service_support::ExternalConnector* connector)
      : service_name_(service_name), connector_(connector) {
    DCHECK(connector_);
    Connect();
  }

  // Reconnect option 2: Provide a callback to re-bind |remote_|. |remote_| is
  // always in an unbound state before |connect_callback_| is run.
  explicit ReconnectingRemote(
      base::RepeatingCallback<void(mojo::Remote<Interface>* remote)>
          connect_callback)
      : connect_callback_(std::move(connect_callback)) {
    Connect();
  }

  // Option 3: Inject an implementation directly to wrap a local implementation.
  // Reconnection is not necessary since a local instance will always exist.
  explicit ReconnectingRemote(Interface* impl) : remote_proxy_(impl) {}

  ReconnectingRemote(const ReconnectingRemote&) = delete;
  ReconnectingRemote& operator=(const ReconnectingRemote&) = delete;
  ~ReconnectingRemote() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  Interface* get() const { return remote_proxy_; }
  Interface* operator->() const { return get(); }
  Interface& operator*() const { return *get(); }

  void OnReconnect(base::RepeatingClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observer_callbacks_.push_back(std::move(callback));
  }

 private:
  void Connect() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    remote_.reset();
    if (connector_) {
      connector_->BindInterface(service_name_,
                                remote_.BindNewPipeAndPassReceiver());
    } else {
      connect_callback_.Run(&remote_);
    }
    DCHECK(remote_.is_bound());
    remote_proxy_ = remote_.get();
    remote_.set_disconnect_handler(
        base::BindOnce(&ReconnectingRemote::Connect, base::Unretained(this)));
    for (auto& callback : observer_callbacks_) {
      callback.Run();
    }
  }

  const std::string service_name_;
  external_service_support::ExternalConnector* const connector_ = nullptr;

  base::RepeatingCallback<void(mojo::Remote<Interface>* remote)>
      connect_callback_;

  mojo::Remote<Interface> remote_;
  Interface* remote_proxy_ = nullptr;
  std::vector<base::RepeatingClosure> observer_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ReconnectingRemote> weak_factory_{this};
};

}  // namespace chromecast
#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_RECONNECTING_REMOTE_H_
