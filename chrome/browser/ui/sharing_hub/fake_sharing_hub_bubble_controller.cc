// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sharing_hub/fake_sharing_hub_bubble_controller.h"

namespace sharing_hub {

FakeSharingHubBubbleController::FakeSharingHubBubbleController(
    std::vector<SharingHubAction> first_party)
    : first_party_actions_(first_party) {}
FakeSharingHubBubbleController::~FakeSharingHubBubbleController() = default;

void FakeSharingHubBubbleController::SetFirstPartyActions(
    std::vector<SharingHubAction> actions) {
  first_party_actions_ = actions;
}

SharingHubBubbleView* FakeSharingHubBubbleController::sharing_hub_bubble_view()
    const {
  return nullptr;
}

bool FakeSharingHubBubbleController::ShouldOfferOmniboxIcon() {
  return true;
}

std::vector<SharingHubAction>
FakeSharingHubBubbleController::GetFirstPartyActions() {
  return first_party_actions_;
}

base::WeakPtr<SharingHubBubbleController>
FakeSharingHubBubbleController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace sharing_hub
