// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PHOSPHOR_BLIND_SIGN_AUTH_FACTORY_IMPL_H_
#define COMPONENTS_PRIVATE_AI_PHOSPHOR_BLIND_SIGN_AUTH_FACTORY_IMPL_H_

#include <memory>

#include "components/private_ai/phosphor/blind_sign_auth_factory.h"

namespace quiche {
class BlindSignAuthInterface;
}  // namespace quiche

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace private_ai::phosphor {

class BlindSignAuthFactoryImpl : public BlindSignAuthFactory {
 public:
  BlindSignAuthFactoryImpl();
  ~BlindSignAuthFactoryImpl() override;

  std::unique_ptr<quiche::BlindSignAuthInterface> CreateBlindSignAuth(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory) override;
};

}  // namespace private_ai::phosphor

#endif  // COMPONENTS_PRIVATE_AI_PHOSPHOR_BLIND_SIGN_AUTH_FACTORY_IMPL_H_
