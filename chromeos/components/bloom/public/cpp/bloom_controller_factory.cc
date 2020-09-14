// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/public/cpp/bloom_controller_factory.h"

#include <memory>

#include "base/callback.h"
#include "chromeos/components/bloom/bloom_controller_impl.h"
#include "chromeos/components/bloom/bloom_interaction_observer_impl.h"
#include "chromeos/components/bloom/bloom_server_proxy_impl.h"
#include "chromeos/components/bloom/public/cpp/bloom_screenshot_delegate.h"

namespace chromeos {
namespace bloom {

// static
std::unique_ptr<BloomController> BloomControllerFactory::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<BloomScreenshotDelegate> screenshot_delegate) {
  auto result = std::make_unique<BloomControllerImpl>(
      identity_manager, std::move(screenshot_delegate),
      std::make_unique<BloomServerProxyImpl>());

  result->AddObserver(std::make_unique<BloomInteractionObserverImpl>());

  return std::move(result);
}

}  // namespace bloom
}  // namespace chromeos
