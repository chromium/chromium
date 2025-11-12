// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SHARING_HUB_FAKE_SHARING_HUB_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_SHARING_HUB_FAKE_SHARING_HUB_BUBBLE_CONTROLLER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sharing_hub {

// A test fake for SharingHubBubbleController. An instance of this class returns
// configurable static data from all of the accessor methods it implements from
// SharingHubBubbleController, and keeps track of whether the callbacks on that
// interface have been invoked or not for tests to query.
class FakeSharingHubBubbleController : public SharingHubBubbleController {
 public:
  explicit FakeSharingHubBubbleController(
      std::vector<SharingHubAction> first_party);
  ~FakeSharingHubBubbleController();

  // Test API:
  void SetFirstPartyActions(std::vector<SharingHubAction> actions);

  // SharingHubBubbleController:
  void HideBubble() override {}
  void ShowBubble(share::ShareAttempt) override {}
  SharingHubBubbleView* sharing_hub_bubble_view() const override;
  bool ShouldOfferOmniboxIcon() override;
  std::vector<SharingHubAction> GetFirstPartyActions() override;
  base::WeakPtr<SharingHubBubbleController> GetWeakPtr() override;

  MOCK_METHOD(void, OnActionSelected, (const SharingHubAction&));
  MOCK_METHOD(void, OnBubbleClosed, ());

 private:
  std::vector<SharingHubAction> first_party_actions_;

  base::WeakPtrFactory<SharingHubBubbleController> weak_factory_{this};
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_SHARING_HUB_FAKE_SHARING_HUB_BUBBLE_CONTROLLER_H_
