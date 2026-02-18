// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PHOSPHOR_BLIND_SIGN_AUTH_FACTORY_H_
#define COMPONENTS_PRIVATE_AI_PHOSPHOR_BLIND_SIGN_AUTH_FACTORY_H_

#include <memory>

#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace private_ai::phosphor {

// Factory for creating `quiche::BlindSignAuthInterface` instances.
class BlindSignAuthFactory {
 public:
  virtual ~BlindSignAuthFactory() = default;

  // Creates a `quiche::BlindSignAuthInterface` instance.
  virtual std::unique_ptr<quiche::BlindSignAuthInterface> CreateBlindSignAuth(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory) = 0;
};

}  // namespace private_ai::phosphor

#endif  // COMPONENTS_PRIVATE_AI_PHOSPHOR_BLIND_SIGN_AUTH_FACTORY_H_
