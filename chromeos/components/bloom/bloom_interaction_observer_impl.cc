// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/bloom_interaction_observer_impl.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "base/check.h"

namespace chromeos {
namespace bloom {

BloomInteractionObserverImpl::BloomInteractionObserverImpl(
    ash::AssistantInteractionController* assistant_interaction_controller)
    : assistant_interaction_controller_(assistant_interaction_controller) {
  DCHECK(assistant_interaction_controller_);
}

BloomInteractionObserverImpl::~BloomInteractionObserverImpl() = default;

void BloomInteractionObserverImpl::OnInteractionStarted() {
  assistant_interaction_controller_->StartBloomInteraction();
}

void BloomInteractionObserverImpl::OnShowUI() {
  // TODO(jeroendh): implement
}

void BloomInteractionObserverImpl::OnShowResult(const std::string& html) {
  // TODO(jeroendh): implement
}

void BloomInteractionObserverImpl::OnInteractionFinished(
    BloomInteractionResolution resolution) {
  // TODO(jeroendh): implement
}

}  // namespace bloom
}  // namespace chromeos
