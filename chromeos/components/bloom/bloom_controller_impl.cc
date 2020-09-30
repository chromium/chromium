// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/bloom_controller_impl.h"

#include "base/logging.h"
#include "chromeos/components/bloom/bloom_interaction.h"
#include "chromeos/components/bloom/public/cpp/bloom_screenshot_delegate.h"
#include "chromeos/components/bloom/public/cpp/bloom_ui_delegate.h"
#include "chromeos/components/bloom/server/bloom_server_proxy.h"

namespace chromeos {
namespace bloom {

BloomControllerImpl::BloomControllerImpl(
    signin::IdentityManager* identity_manager,
    std::unique_ptr<BloomServerProxy> server_proxy)
    : identity_manager_(identity_manager),
      screenshot_delegate_(nullptr),
      ui_delegate_(nullptr),
      server_proxy_(std::move(server_proxy)) {
  DCHECK(identity_manager_);
  DCHECK(server_proxy_);
}

BloomControllerImpl::~BloomControllerImpl() = default;

void BloomControllerImpl::SetScreenshotDelegate(
    std::unique_ptr<BloomScreenshotDelegate> value) {
  DCHECK(value);
  screenshot_delegate_ = std::move(value);
}

void BloomControllerImpl::SetUiDelegate(
    std::unique_ptr<BloomUiDelegate> value) {
  DCHECK(value);
  ui_delegate_ = std::move(value);
}

void BloomControllerImpl::StartInteraction() {
  DCHECK(screenshot_delegate_);
  DCHECK(ui_delegate_);
  DCHECK(!current_interaction_);

  DVLOG(1) << "Starting Bloom interaction";

  current_interaction_ = std::make_unique<BloomInteraction>(this);
  current_interaction_->Start();

  ui_delegate_->OnInteractionStarted();
}

bool BloomControllerImpl::HasInteraction() const {
  return current_interaction_ != nullptr;
}

void BloomControllerImpl::ShowUI() {
  ui_delegate_->OnShowUI();
}

void BloomControllerImpl::ShowResult(const std::string& result) {
  ui_delegate_->OnShowResult(result);
}

void BloomControllerImpl::StopInteraction(
    BloomInteractionResolution resolution) {
  DCHECK(current_interaction_);

  DVLOG(1) << "Stopping Bloom interaction with resolution "
           << ToString(resolution);

  current_interaction_ = nullptr;

  ui_delegate_->OnInteractionFinished(resolution);
}

}  // namespace bloom
}  // namespace chromeos
