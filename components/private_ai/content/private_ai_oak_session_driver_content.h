// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CONTENT_PRIVATE_AI_OAK_SESSION_DRIVER_CONTENT_H_
#define COMPONENTS_PRIVATE_AI_CONTENT_PRIVATE_AI_OAK_SESSION_DRIVER_CONTENT_H_

#include "components/private_ai/mojom/oak_session.mojom.h"
#include "components/private_ai/private_ai_oak_session_driver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace private_ai {

// Desktop implementation of the PrivateAiOakSessionDriver.
class PrivateAiOakSessionDriverContent : public PrivateAiOakSessionDriver {
 public:
  PrivateAiOakSessionDriverContent() = default;
  ~PrivateAiOakSessionDriverContent() override = default;

  PrivateAiOakSessionDriverContent(const PrivateAiOakSessionDriverContent&) =
      delete;
  PrivateAiOakSessionDriverContent& operator=(
      const PrivateAiOakSessionDriverContent&) = delete;

  // PrivateAiOakSessionDriver overrides:
  mojo::Remote<mojom::OakSession> BindOakSessionService() override;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CONTENT_PRIVATE_AI_OAK_SESSION_DRIVER_CONTENT_H_
