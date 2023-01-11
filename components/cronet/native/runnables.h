// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_RUNNABLES_H_
#define COMPONENTS_CRONET_NATIVE_RUNNABLES_H_

#include "base/functional/callback.h"
#include "components/cronet/native/generated/cronet.idl_impl_interface.h"

namespace cronet {

// Implementation of CronetRunnable that runs arbitrary base::OnceClosure.
// Runnable destroys itself after execution.
class OnceClosureRunnable : public Cronet_Runnable {
 public:
  explicit OnceClosureRunnable(base::OnceClosure task);

  OnceClosureRunnable(const OnceClosureRunnable&) = delete;
  OnceClosureRunnable& operator=(const OnceClosureRunnable&) = delete;

  ~OnceClosureRunnable() override;

  void Run() override;

 private:
  // Closure to run.
  base::OnceClosure task_;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_NATIVE_RUNNABLES_H_
