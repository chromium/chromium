// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/bloom_controller_impl.h"

#include "base/logging.h"
#include "chromeos/components/bloom/bloom_interaction.h"
#include "chromeos/components/bloom/bloom_server_proxy.h"
#include "chromeos/components/bloom/public/cpp/bloom_screenshot_delegate.h"

namespace chromeos {
namespace bloom {

BloomControllerImpl::BloomControllerImpl(
    signin::IdentityManager* identity_manager,
    std::unique_ptr<BloomScreenshotDelegate> screenshot_delegate,
    std::unique_ptr<BloomServerProxy> server_proxy)
    : identity_manager_(identity_manager),
      screenshot_delegate_(std::move(screenshot_delegate)),
      server_proxy_(std::move(server_proxy)) {
  DCHECK(identity_manager_);
  DCHECK(screenshot_delegate_);
  DCHECK(server_proxy_);
}

BloomControllerImpl::~BloomControllerImpl() = default;

void BloomControllerImpl::StartInteraction() {
  DCHECK(!current_interaction_);

  DVLOG(1) << "Starting Bloom interaction";

  current_interaction_ = std::make_unique<BloomInteraction>(this);
  current_interaction_->Start();

  for (auto& observer : interaction_observers_)
    observer.OnInteractionStarted();
}

bool BloomControllerImpl::HasInteraction() const {
  return current_interaction_ != nullptr;
}

void BloomControllerImpl::ShowUI() {
  for (auto& observer : interaction_observers_)
    observer.OnShowUI();
}

void BloomControllerImpl::ShowResult(const std::string& result) {
  for (auto& observer : interaction_observers_)
    observer.OnShowResult(result);
}

void BloomControllerImpl::StopInteraction(
    BloomInteractionResolution resolution) {
  DCHECK(current_interaction_);

  DVLOG(1) << "Stopping Bloom interaction with resolution "
           << ToString(resolution);

  current_interaction_ = nullptr;

  for (auto& observer : interaction_observers_)
    observer.OnInteractionFinished(resolution);
}

void BloomControllerImpl::AddObserver(BloomInteractionObserver* observer) {
  DCHECK(observer);
  interaction_observers_.AddObserver(observer);
}

void BloomControllerImpl::AddObserver(
    std::unique_ptr<BloomInteractionObserver> observer) {
  AddObserver(observer.get());
  owned_interaction_observers_.push_back(std::move(observer));
}

void BloomControllerImpl::SetScreenshotDelegateForTesting(
    std::unique_ptr<BloomScreenshotDelegate> screenshot_delegate) {
  DCHECK(screenshot_delegate);
  screenshot_delegate_ = std::move(screenshot_delegate);
}

}  // namespace bloom
}  // namespace chromeos
