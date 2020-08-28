// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/bloom_controller_impl.h"

#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "chromeos/components/bloom/bloom_interaction.h"
#include "chromeos/components/bloom/screenshot_grabber.h"

namespace chromeos {
namespace bloom {

BloomControllerImpl::BloomControllerImpl(
    signin::IdentityManager* identity_manager,
    ash::AssistantInteractionController* assistant_interaction_controller,
    std::unique_ptr<ScreenshotGrabber> screenshot_grabber)
    : identity_manager_(identity_manager),
      assistant_interaction_controller_(assistant_interaction_controller),
      screenshot_grabber_(std::move(screenshot_grabber)) {
  DCHECK(identity_manager_);
  DCHECK(assistant_interaction_controller_);
  DCHECK(screenshot_grabber_);
}

BloomControllerImpl::~BloomControllerImpl() = default;

void BloomControllerImpl::StartInteraction() {
  DCHECK(!current_interaction_);

  DVLOG(1) << "Starting Bloom interaction";

  current_interaction_ = std::make_unique<BloomInteraction>(this);
  current_interaction_->Start();
}

bool BloomControllerImpl::HasInteraction() const {
  return current_interaction_ != nullptr;
}

void BloomControllerImpl::StopInteraction(
    BloomInteractionResolution resolution) {
  DCHECK(current_interaction_);

  DVLOG(1) << "Stopping Bloom interaction with resolution "
           << ToString(resolution);

  current_interaction_ = nullptr;
  last_interaction_resolution_ = resolution;
}

BloomInteractionResolution BloomControllerImpl::GetLastInteractionResolution()
    const {
  return last_interaction_resolution_;
}

void BloomControllerImpl::SetScreenshotGrabberForTesting(
    std::unique_ptr<ScreenshotGrabber> screenshot_grabber) {
  DCHECK(screenshot_grabber);
  screenshot_grabber_ = std::move(screenshot_grabber);
}

}  // namespace bloom
}  // namespace chromeos
