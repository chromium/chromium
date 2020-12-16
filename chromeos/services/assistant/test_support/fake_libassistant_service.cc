// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/test_support/fake_libassistant_service.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

FakeLibassistantService::FakeLibassistantService() : receiver_(this) {}

FakeLibassistantService::~FakeLibassistantService() = default;

void FakeLibassistantService::Bind(
    mojo::PendingReceiver<libassistant::mojom::LibassistantService>
        pending_receiver) {
  EXPECT_FALSE(receiver_.is_bound())
      << "Cannot bind the LibassistantService twice";
  receiver_.Bind(std::move(pending_receiver));
}

void FakeLibassistantService::Unbind() {
  receiver_.reset();
  service_controller().Unbind();
}

void FakeLibassistantService::BindServiceController(
    mojo::PendingReceiver<libassistant::mojom::ServiceController> receiver) {
  service_controller_.Bind(std::move(receiver));
}

}  // namespace assistant
}  // namespace chromeos
