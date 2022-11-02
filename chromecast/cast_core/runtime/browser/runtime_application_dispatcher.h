// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_

#include "components/cast_receiver/common/public/status.h"

namespace chromecast {

class RuntimeApplicationDispatcher {
 public:
  // |application_client| is expected to persist for the lifetime of this
  // instance.
  virtual ~RuntimeApplicationDispatcher();

  // Starts and stops the runtime service, including the gRPC completion queue.
  virtual cast_receiver::Status Start() = 0;
  virtual cast_receiver::Status Stop() = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_
