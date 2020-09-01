// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_BLOOM_CONTROLLER_IMPL_H_
#define CHROMEOS_COMPONENTS_BLOOM_BLOOM_CONTROLLER_IMPL_H_

#include <memory>
#include <vector>

#include "base/observer_list.h"
#include "chromeos/components/bloom/bloom_interaction_observer.h"
#include "chromeos/components/bloom/public/cpp/bloom_controller.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace chromeos {
namespace bloom {

class BloomInteraction;
class BloomServerProxy;
class ScreenshotGrabber;

class BloomControllerImpl : public BloomController {
 public:
  BloomControllerImpl(signin::IdentityManager* identity_manager,
                      std::unique_ptr<ScreenshotGrabber> screenshot_grabber,
                      std::unique_ptr<BloomServerProxy> server_proxy);
  BloomControllerImpl(const BloomControllerImpl&) = delete;
  BloomControllerImpl& operator=(const BloomControllerImpl&) = delete;
  ~BloomControllerImpl() override;

  // BloomController implementation:
  void StartInteraction() override;
  void StopInteraction(BloomInteractionResolution resolution) override;

  void AddObserver(BloomInteractionObserver* observer) override;
  void AddObserver(std::unique_ptr<BloomInteractionObserver> observer) override;

  void ShowUI();
  void ShowResult(const std::string& result);

  ScreenshotGrabber* screenshot_grabber() { return screenshot_grabber_.get(); }
  BloomServerProxy* server_proxy() { return server_proxy_.get(); }
  signin::IdentityManager* identity_manager() { return identity_manager_; }

  void SetScreenshotGrabberForTesting(std::unique_ptr<ScreenshotGrabber>);

 private:
  signin::IdentityManager* const identity_manager_;
  std::unique_ptr<ScreenshotGrabber> screenshot_grabber_;
  std::unique_ptr<BloomServerProxy> server_proxy_;

  base::ObserverList<BloomInteractionObserver> interaction_observers_;
  std::vector<std::unique_ptr<BloomInteractionObserver>>
      owned_interaction_observers_;

  std::unique_ptr<BloomInteraction> current_interaction_;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_BLOOM_CONTROLLER_IMPL_H_
