// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/public/cpp/bloom_controller_factory.h"

#include <memory>

#include "base/callback.h"
#include "chromeos/components/bloom/bloom_controller_impl.h"
#include "chromeos/components/bloom/bloom_interaction_observer_impl.h"
#include "chromeos/components/bloom/bloom_server_proxy_impl.h"
#include "chromeos/components/bloom/screenshot_grabber.h"

namespace chromeos {
namespace bloom {

namespace {

// TODO(jeroendh): Replace with actual working screenshot grabber.
class FakeScreenshotGrabber : public ScreenshotGrabber {
 public:
  void TakeScreenshot(Callback callback) override { NOTIMPLEMENTED(); }
};

}  // namespace

// static
std::unique_ptr<BloomController> BloomControllerFactory::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager) {
  auto result = std::make_unique<BloomControllerImpl>(
      identity_manager, std::make_unique<FakeScreenshotGrabber>(),
      std::make_unique<BloomServerProxyImpl>());

  result->AddObserver(std::make_unique<BloomInteractionObserverImpl>());

  return std::move(result);
}

}  // namespace bloom
}  // namespace chromeos
