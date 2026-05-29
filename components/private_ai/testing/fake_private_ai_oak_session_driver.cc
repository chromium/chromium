// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/testing/fake_private_ai_oak_session_driver.h"

namespace private_ai {

FakePrivateAiOakSessionDriver::FakePrivateAiOakSessionDriver() = default;

FakePrivateAiOakSessionDriver::FakePrivateAiOakSessionDriver(
    mojo::Remote<mojom::OakSession> service)
    : service_(std::move(service)) {}

FakePrivateAiOakSessionDriver::~FakePrivateAiOakSessionDriver() = default;

mojo::Remote<mojom::OakSession>
FakePrivateAiOakSessionDriver::BindOakSessionService() {
  return std::move(service_);
}

}  // namespace private_ai
