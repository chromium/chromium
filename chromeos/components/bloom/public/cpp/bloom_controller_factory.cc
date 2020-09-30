// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/public/cpp/bloom_controller_factory.h"

#include <memory>

#include "base/callback.h"
#include "chromeos/components/bloom/bloom_controller_impl.h"
#include "chromeos/components/bloom/public/cpp/bloom_ui_controller.h"
#include "chromeos/components/bloom/public/cpp/bloom_ui_delegate.h"
#include "chromeos/components/bloom/server/bloom_server_proxy_impl.h"
#include "chromeos/components/bloom/server/bloom_url_loader_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {
namespace bloom {

namespace {

// Wrapper class that will forward all requests to the |BloomUiController|,
// and handles the case where the controller is nullptr.
class BloomUiDelegateWrapper : public BloomUiDelegate {
 public:
  BloomUiDelegateWrapper() = default;
  BloomUiDelegateWrapper(BloomUiDelegateWrapper&) = delete;
  BloomUiDelegateWrapper& operator=(BloomUiDelegateWrapper&) = delete;
  ~BloomUiDelegateWrapper() override = default;

  void OnInteractionStarted() override {
    if (!delegate())
      return;
    return delegate()->OnInteractionStarted();
  }

  void OnShowUI() override {
    if (!delegate())
      return;
    return delegate()->OnShowUI();
  }

  void OnShowResult(const std::string& html) override {
    if (!delegate())
      return;
    return delegate()->OnShowResult(html);
  }

  void OnInteractionFinished(BloomInteractionResolution resolution) override {
    if (!delegate())
      return;
    return delegate()->OnInteractionFinished(resolution);
  }

 private:
  BloomUiDelegate* delegate() {
    BloomUiController* controller = BloomUiController::Get();
    if (controller)
      return &controller->GetUiDelegate();
    return nullptr;
  }
};

}  // namespace

// static
std::unique_ptr<BloomController> BloomControllerFactory::Create(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager) {
  auto result = std::make_unique<BloomControllerImpl>(
      identity_manager,
      std::make_unique<BloomServerProxyImpl>(
          std::make_unique<BloomURLLoaderImpl>(std::move(url_loader_factory))));

  result->SetUiDelegate(std::make_unique<BloomUiDelegateWrapper>());

  return result;
}

}  // namespace bloom
}  // namespace chromeos
