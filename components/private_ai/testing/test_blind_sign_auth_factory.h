// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TESTING_TEST_BLIND_SIGN_AUTH_FACTORY_H_
#define COMPONENTS_PRIVATE_AI_TESTING_TEST_BLIND_SIGN_AUTH_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/private_ai/phosphor/blind_sign_auth_factory.h"
#include "components/private_ai/phosphor/mock_blind_sign_auth.h"

namespace network {
class PendingSharedURLLoaderFactory;
}

namespace quiche {
class BlindSignAuthInterface;
}

namespace private_ai {

class TestBlindSignAuthFactory : public phosphor::BlindSignAuthFactory {
 public:
  TestBlindSignAuthFactory();
  ~TestBlindSignAuthFactory() override;

  std::unique_ptr<quiche::BlindSignAuthInterface> CreateBlindSignAuth(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory) override;

  phosphor::MockBlindSignAuth* mock_bsa() { return bsa_; }
  void ResetBsa() { bsa_ = nullptr; }

 private:
  raw_ptr<phosphor::MockBlindSignAuth> bsa_ = nullptr;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TESTING_TEST_BLIND_SIGN_AUTH_FACTORY_H_
