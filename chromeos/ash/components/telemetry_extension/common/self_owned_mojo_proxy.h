// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_COMMON_SELF_OWNED_MOJO_PROXY_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_COMMON_SELF_OWNED_MOJO_PROXY_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class SelfOwnedMojoProxyInterface {
 public:
  SelfOwnedMojoProxyInterface(const SelfOwnedMojoProxyInterface&) = delete;
  SelfOwnedMojoProxyInterface& operator=(const SelfOwnedMojoProxyInterface&) =
      delete;
  virtual ~SelfOwnedMojoProxyInterface() = default;

  virtual void OnServiceDestroyed() = 0;

 protected:
  SelfOwnedMojoProxyInterface() = default;
};

// This class handles the lifetime for proxy interface implementations i.e.
// implementations that forward calls between two Mojo interfaces. The
// two interfaces are represented by the types `RemoteInterface` and
// `ReceiverInterface` for the `mojo::Remote` and `mojo::Receiver` respectively.
// This class owns all components of the connection, which are:
// - the `mojo::Receiver`,
// - the interface implementation of the receiver (which has a
// `mojo::PendingRemote` as a constructor parameter). This class handles
// disconnections on both Mojom pipes and forwards it to the other pipe. Classes
// that keep pointers to this class can get notified of its deletion by passing
// a `OnDisconnectCallback`. After this callback runs, this class will delete
// itself.
template <typename RemoteInterface,
          typename ReceiverInterface,
          typename ReceiverImpl>
class SelfOwnedMojoProxy : public SelfOwnedMojoProxyInterface {
 public:
  using OnDisconnectCallback =
      base::OnceCallback<void(base::WeakPtr<SelfOwnedMojoProxyInterface>)>;

  template <typename... ImplArgs>
  static base::WeakPtr<SelfOwnedMojoProxyInterface> Create(
      mojo::PendingReceiver<ReceiverInterface> pending_receiver,
      mojo::PendingRemote<RemoteInterface> pending_remote,
      OnDisconnectCallback on_disconnect_callback,
      ImplArgs... impl_args) {
    auto impl =
        std::make_unique<ReceiverImpl>(std::move(pending_remote), impl_args...);
    auto self_owned_mojo_proxy =
        new SelfOwnedMojoProxy(std::move(impl), std::move(pending_receiver),
                               std::move(on_disconnect_callback));
    return self_owned_mojo_proxy->GetWeakPtr();
  }

  SelfOwnedMojoProxy(const SelfOwnedMojoProxy&) = delete;
  SelfOwnedMojoProxy& operator=(const SelfOwnedMojoProxy&) = delete;
  ~SelfOwnedMojoProxy() = default;

  void OnServiceDestroyed() override {
    // SAFETY: We can do this since the only way to create an instance is
    // through the `Create` method that uses `new`.
    delete this;
  }

 private:
  friend base::WeakPtr<SelfOwnedMojoProxy> Create(
      std::unique_ptr<ReceiverImpl> receiver_impl,
      mojo::PendingReceiver<ReceiverInterface> pending_receiver,
      OnDisconnectCallback on_disconnect_callback);

  explicit SelfOwnedMojoProxy(
      std::unique_ptr<ReceiverImpl> receiver_impl,
      mojo::PendingReceiver<ReceiverInterface> pending_receiver,
      OnDisconnectCallback on_disconnect_callback)
      : receiver_impl_(std::move(receiver_impl)),
        receiver_(receiver_impl_.get(), std::move(pending_receiver)),
        on_disconnect_(std::move(on_disconnect_callback)) {
    // SAFETY: We can use base::Unretained here since we own the receiver as
    // well as the impl that holds the remote.
    receiver_.set_disconnect_with_reason_handler(base::BindOnce(
        &SelfOwnedMojoProxy::OnReceiverDisconnect, base::Unretained(this)));
    receiver_impl_->GetRemote().set_disconnect_with_reason_handler(
        base::BindOnce(&SelfOwnedMojoProxy::OnRemoteDisconnect,
                       base::Unretained(this)));
  }

  // Called when the pipe to `RemoteInterface` is closed. This results in
  // closing the pipe to `ReceiverInterface` and calling the
  // on_disconnect_callback.
  void OnRemoteDisconnect(uint32_t error_code, const std::string& custom_msg) {
    receiver_.ResetWithReason(error_code, custom_msg);
    // Results in the destruction of `this`, nothing should be called
    // afterwards.
    NotifyOnDisconnect();
  }

  // Called when the pipe to `ReceiverInterface` is closed. This results in
  // closing the pipe to `RemoteInterface` and calling the
  // on_disconnect_callback.
  void OnReceiverDisconnect(uint32_t error_code,
                            const std::string& custom_msg) {
    receiver_impl_->GetRemote().ResetWithReason(error_code, custom_msg);
    // Results in the destruction of `this`, nothing should be called
    // afterwards.
    NotifyOnDisconnect();
  }

  void NotifyOnDisconnect() {
    std::move(on_disconnect_).Run(GetWeakPtr());
    // SAFETY: We can do this since the only way to create an instance is
    // through the `Create` method that uses `new`.
    delete this;
  }

  base::WeakPtr<SelfOwnedMojoProxy> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // The order matters for destruction.
  std::unique_ptr<ReceiverImpl> receiver_impl_;
  mojo::Receiver<ReceiverInterface> receiver_;

  // Called when the connection is reset from either side.
  OnDisconnectCallback on_disconnect_;

  // Must be the last member of the class.
  base::WeakPtrFactory<SelfOwnedMojoProxy> weak_ptr_factory_{this};
};

// Comparator for `base::WeakPtr<SelfOwnedMojoProxyInterface>`.
struct SelfOwnedMojoProxyInterfaceWeakPtrComparator {
  bool operator()(const base::WeakPtr<SelfOwnedMojoProxyInterface>& a,
                  const base::WeakPtr<SelfOwnedMojoProxyInterface>& b) const {
    return a.get() < b.get();
  }
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_COMMON_SELF_OWNED_MOJO_PROXY_H_
