// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CONNECTION_FACTORY_H_
#define COMPONENTS_LEGION_CONNECTION_FACTORY_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/legion/connection.h"

namespace private_ai {

// Interface for creating `Connection` instances.
class ConnectionFactory {
 public:
  virtual ~ConnectionFactory() = default;

  // Creates a new `Connection` instance.
  //
  // `on_disconnect` is invoked when the connection is disconnected. Sending
  // requests to disconnected connection will result in an error.
  virtual std::unique_ptr<Connection> Create(
      base::RepeatingClosure on_disconnect) = 0;
};

}  // namespace private_ai

#endif  // COMPONENTS_LEGION_CONNECTION_FACTORY_H_
