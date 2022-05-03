// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_DESKTOP_IMPL_H_
#define CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_DESKTOP_IMPL_H_

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/models/image_model.h"

class Profile;

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
      public content::WebContentsObserver,
      public content::WebContentsUserData<
          SharingHubBubbleControllerDesktopImpl>,
      public base::SupportsWeakPtr<SharingHubBubbleControllerDesktopImpl> {
 public:
  using PreviewImageChangedCallback =
      base::RepeatingCallback<void(ui::ImageModel)>;

  SharingHubBubbleControllerDesktopImpl(
      const SharingHubBubbleControllerDesktopImpl&) = delete;
  SharingHubBubbleControllerDesktopImpl& operator=(
      const SharingHubBubbleControllerDesktopImpl&) = delete;

  ~SharingHubBubbleControllerDesktopImpl() override;

  // SharingHubBubbleController:
  void HideBubble() override;
  void ShowBubble() override;
  SharingHubBubbleView* sharing_hub_bubble_view() const override;
  bool ShouldOfferOmniboxIcon() override;

  // Returns the title of the Sharing Hub bubble.
  std::u16string GetWindowTitle() const;
  // Returns the current profile.
  Profile* GetProfile() const;

  // Returns the list of Sharing Hub first party actions.
  virtual std::vector<SharingHubAction> GetFirstPartyActions();
  // Returns the list of Sharing Hub third party actions.
  virtual std::vector<SharingHubAction> GetThirdPartyActions();

  virtual bool ShouldUsePreview();
  virtual std::u16string GetPreviewTitle();
  virtual GURL GetPreviewUrl();
  virtual ui::ImageModel GetPreviewImage();

  base::CallbackListSubscription RegisterPreviewImageChangedCallback(
      PreviewImageChangedCallback callback);

  // Handles when the user clicks on a Sharing Hub action. If this is a first
  // party action, executes the appropriate browser command. If this is a third
  // party action, navigates to an external webpage.
  virtual void OnActionSelected(int command_id,
                                bool is_first_party,
                                std::string feature_name_for_metrics);
  // Handler for when the bubble is closed.
  void OnBubbleClosed();

 protected:
  explicit SharingHubBubbleControllerDesktopImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      SharingHubBubbleControllerDesktopImpl>;

  SharingHubModel* GetSharingHubModel();

  // Weak reference. Will be nullptr if no bubble is currently shown.
  raw_ptr<SharingHubBubbleView> sharing_hub_bubble_view_ = nullptr;
  // Cached reference to the model.
  raw_ptr<SharingHubModel> sharing_hub_model_ = nullptr;

  base::RepeatingCallbackList<void(ui::ImageModel)>
      preview_image_changed_callbacks_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_DESKTOP_IMPL_H_
