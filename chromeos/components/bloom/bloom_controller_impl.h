// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_BLOOM_CONTROLLER_IMPL_H_
#define CHROMEOS_COMPONENTS_BLOOM_BLOOM_CONTROLLER_IMPL_H_

#include <memory>

#include "chromeos/components/bloom/public/cpp/bloom_controller.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace chromeos {
namespace bloom {

class BloomInteraction;
class BloomScreenshotDelegate;
class BloomServerProxy;
class BloomUiDelegate;

class BloomControllerImpl : public BloomController {
 public:
  BloomControllerImpl(signin::IdentityManager* identity_manager,
                      std::unique_ptr<BloomServerProxy> server_proxy);
  BloomControllerImpl(const BloomControllerImpl&) = delete;
  BloomControllerImpl& operator=(const BloomControllerImpl&) = delete;
  ~BloomControllerImpl() override;

  // BloomController implementation:
  void StartInteraction() override;
  bool HasInteraction() const override;
  void StopInteraction(BloomInteractionResolution resolution) override;

  void ShowUI();
  void ShowResult(const std::string& result);

  void SetScreenshotDelegate(
      std::unique_ptr<BloomScreenshotDelegate> delegate);
  void SetUiDelegate(std::unique_ptr<BloomUiDelegate> delegate);

  BloomScreenshotDelegate* screenshot_delegate() {
    return screenshot_delegate_.get();
  }
  BloomServerProxy* server_proxy() { return server_proxy_.get(); }
  signin::IdentityManager* identity_manager() { return identity_manager_; }

 private:
  signin::IdentityManager* const identity_manager_;
  std::unique_ptr<BloomScreenshotDelegate> screenshot_delegate_;
  std::unique_ptr<BloomUiDelegate> ui_delegate_;
  std::unique_ptr<BloomServerProxy> server_proxy_;

  std::unique_ptr<BloomInteraction> current_interaction_;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_BLOOM_CONTROLLER_IMPL_H_
