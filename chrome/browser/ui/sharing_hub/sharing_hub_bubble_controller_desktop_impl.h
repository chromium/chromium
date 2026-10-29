// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_DESKTOP_IMPL_H_
#define CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_DESKTOP_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/models/image_model.h"

namespace content {
class WebContents;
}  // namespace content

namespace sharing_hub {

class SharingHubBubbleView;
class SharingHubModel;
struct SharingHubAction;

// Controller component of the Sharing Hub dialog bubble.
// Responsible for showing and hiding an owned bubble.
class SharingHubBubbleControllerDesktopImpl
    : public SharingHubBubbleController,
      public content::WebContentsUserData<
          SharingHubBubbleControllerDesktopImpl> {
 public:
  SharingHubBubbleControllerDesktopImpl(
      const SharingHubBubbleControllerDesktopImpl&) = delete;
  SharingHubBubbleControllerDesktopImpl& operator=(
      const SharingHubBubbleControllerDesktopImpl&) = delete;

  ~SharingHubBubbleControllerDesktopImpl() override;

  // SharingHubBubbleController:
  void HideBubble() override;
  void ShowBubble(share::ShareAttempt attempt) override;
  SharingHubBubbleView* sharing_hub_bubble_view() const override;
  bool ShouldOfferOmniboxIcon() override;
  std::vector<SharingHubAction> GetFirstPartyActions() override;
  base::WeakPtr<SharingHubBubbleController> GetWeakPtr() override;
  void OnActionSelected(const SharingHubAction& action) override;
  void OnBubbleClosed() override;

 protected:
  explicit SharingHubBubbleControllerDesktopImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      SharingHubBubbleControllerDesktopImpl>;

  SharingHubModel* GetSharingHubModel();

  // This method can return an empty preview image if the site doesn't have a
  // favicon and we can't generate a preview image for some reason.
  ui::ImageModel GetPreviewImage();

  // Weak reference. Will be nullptr if no bubble is currently shown.
  raw_ptr<SharingHubBubbleView> sharing_hub_bubble_view_ = nullptr;
  // Cached reference to the model.
  raw_ptr<SharingHubModel> sharing_hub_model_ = nullptr;

  base::WeakPtrFactory<SharingHubBubbleController> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_DESKTOP_IMPL_H_
