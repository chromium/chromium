// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/testing/test_blind_sign_auth_factory.h"

#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace private_ai {

TestBlindSignAuthFactory::TestBlindSignAuthFactory() = default;
TestBlindSignAuthFactory::~TestBlindSignAuthFactory() = default;

std::unique_ptr<quiche::BlindSignAuthInterface>
TestBlindSignAuthFactory::CreateBlindSignAuth(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory) {
  auto bsa = std::make_unique<phosphor::MockBlindSignAuth>();
  bsa_ = bsa.get();
  return bsa;
}

}  // namespace private_ai
