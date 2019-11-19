// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_MOJO_CHANNEL_H_
#define COMPONENTS_ARC_SESSION_MOJO_CHANNEL_H_

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "components/arc/session/connection_holder.h"

namespace arc {

// Thin interface to wrap InterfacePtr<T> with type erasure.
class MojoChannelBase {
 public:
  virtual ~MojoChannelBase() = default;

 protected:
  MojoChannelBase() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoChannelBase);
};

// Thin wrapper for InterfacePtr<T>, where T is one of ARC mojo Instance class.
template <typename InstanceType, typename HostType>
class MojoChannel : public MojoChannelBase {
 public:
  MojoChannel(ConnectionHolder<InstanceType, HostType>* holder,
              mojo::InterfacePtr<InstanceType> ptr)
      : holder_(holder), ptr_(std::move(ptr)) {
    // Delay registration to the ConnectionHolder until the version is ready.
  }

  ~MojoChannel() override { holder_->CloseInstance(ptr_.get()); }

  void set_connection_error_handler(base::OnceClosure error_handler) {
    ptr_.set_connection_error_handler(std::move(error_handler));
  }

  void QueryVersion() {
    // Note: the callback will not be called if |ptr_| is destroyed.
    ptr_.QueryVersion(
        base::Bind(&MojoChannel::OnVersionReady, base::Unretained(this)));
  }

 private:
  void OnVersionReady(uint32_t unused_version) {
    holder_->SetInstance(ptr_.get(), ptr_.version());
  }

  // Externally owned ConnectionHolder instance.
  ConnectionHolder<InstanceType, HostType>* const holder_;

  // Put as a last member to ensure that any callback tied to the |ptr_|
  // is not invoked.
  mojo::InterfacePtr<InstanceType> ptr_;

  DISALLOW_COPY_AND_ASSIGN(MojoChannel);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_MOJO_CHANNEL_H_
