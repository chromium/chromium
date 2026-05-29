// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TESTING_FAKE_PRIVATE_AI_OAK_SESSION_DRIVER_H_
#define COMPONENTS_PRIVATE_AI_TESTING_FAKE_PRIVATE_AI_OAK_SESSION_DRIVER_H_

#include "components/private_ai/mojom/oak_session.mojom.h"
#include "components/private_ai/private_ai_oak_session_driver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace private_ai {

class FakePrivateAiOakSessionDriver : public PrivateAiOakSessionDriver {
 public:
  FakePrivateAiOakSessionDriver();
  explicit FakePrivateAiOakSessionDriver(
      mojo::Remote<mojom::OakSession> service);
  ~FakePrivateAiOakSessionDriver() override;

  mojo::Remote<mojom::OakSession> BindOakSessionService() override;

 private:
  mojo::Remote<mojom::OakSession> service_;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TESTING_FAKE_PRIVATE_AI_OAK_SESSION_DRIVER_H_
