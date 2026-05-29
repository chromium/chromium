// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PRIVATE_AI_OAK_SESSION_DRIVER_H_
#define COMPONENTS_PRIVATE_AI_PRIVATE_AI_OAK_SESSION_DRIVER_H_

#include "components/private_ai/mojom/oak_session.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace private_ai {

// Abstract interface for providing platform-specific implementations of
// capabilities required by the Private AI component for OakSessionService.
class PrivateAiOakSessionDriver {
 public:
  virtual ~PrivateAiOakSessionDriver() = default;

  // Launches or binds the OakSessionService, abstracting the platform-specific
  // process.
  virtual mojo::Remote<mojom::OakSession> BindOakSessionService() = 0;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_PRIVATE_AI_OAK_SESSION_DRIVER_H_
