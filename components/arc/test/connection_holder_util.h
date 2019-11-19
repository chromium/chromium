// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_CONNECTION_HOLDER_UTIL_H_
#define COMPONENTS_ARC_TEST_CONNECTION_HOLDER_UTIL_H_

#include <utility>

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/arc/session/connection_holder.h"

namespace arc {

namespace internal {

// An observer that runs a closure when the connection is ready.
template <typename InstanceType, typename HostType>
class ReadinessObserver
    : public ConnectionHolder<InstanceType, HostType>::Observer {
 public:
  ReadinessObserver(ConnectionHolder<InstanceType, HostType>* holder,
                    base::OnceClosure closure)
      : holder_(holder), closure_(std::move(closure)) {
    holder_->AddObserver(this);
  }
  ~ReadinessObserver() override { holder_->RemoveObserver(this); }

 private:
  void OnConnectionReady() override {
    if (!closure_)
      return;
    std::move(closure_).Run();
  }

  ConnectionHolder<InstanceType, HostType>* const holder_;  // Owned by caller
  base::OnceClosure closure_;

  DISALLOW_COPY_AND_ASSIGN(ReadinessObserver);
};

}  // namespace internal

// Waits for the instance to be ready.
template <typename InstanceType, typename HostType>
void WaitForInstanceReady(ConnectionHolder<InstanceType, HostType>* holder) {
  if (holder->IsConnected())
    return;

  base::RunLoop run_loop;
  internal::ReadinessObserver<InstanceType, HostType> readiness_observer(
      holder, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_CONNECTION_HOLDER_UTIL_H_
