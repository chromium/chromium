// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/phosphor/blind_sign_auth_factory_impl.h"

#include <memory>
#include <utility>

#include "components/private_ai/phosphor/config_http.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/blind_sign_auth_options.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace private_ai::phosphor {

BlindSignAuthFactoryImpl::BlindSignAuthFactoryImpl() = default;
BlindSignAuthFactoryImpl::~BlindSignAuthFactoryImpl() = default;

std::unique_ptr<quiche::BlindSignAuthInterface>
BlindSignAuthFactoryImpl::CreateBlindSignAuth(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory) {
  privacy::ppn::BlindSignAuthOptions bsa_options{};
  bsa_options.set_enable_privacy_pass(true);

  return std::make_unique<quiche::BlindSignAuth>(
      std::make_unique<ConfigHttp>(std::move(pending_url_loader_factory)),
      std::move(bsa_options));
}

}  // namespace private_ai::phosphor
