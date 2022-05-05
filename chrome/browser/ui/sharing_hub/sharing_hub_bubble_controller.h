// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/models/image_model.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chromeos/components/sharesheet/constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Button;
}  // namespace views

namespace sharesheet {
class SharesheetService;
}  // namespace sharesheet

namespace sharing_hub {

class SharingHubBubbleView;
class SharingHubModel;
struct SharingHubAction;

// Controller component of the Sharing Hub dialog bubble.
// Responsible for showing and hiding an owned bubble.
class SharingHubBubbleController
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SharingHubBubbleController>,
      public base::SupportsWeakPtr<SharingHubBubbleController> {
 public:
  using PreviewImageChangedCallback =
      base::RepeatingCallback<void(ui::ImageModel)>;

  SharingHubBubbleController(const SharingHubBubbleController&) = delete;
  SharingHubBubbleController& operator=(const SharingHubBubbleController&) =
      delete;

  ~SharingHubBubbleController() override;

  static SharingHubBubbleController* CreateOrGetFromWebContents(
      content::WebContents* web_contents);

  // Hides the Sharing Hub bubble.
  void HideBubble();
  // Displays the Sharing Hub bubble.
  void ShowBubble();

  // Returns nullptr if no bubble is currently shown.
  SharingHubBubbleView* sharing_hub_bubble_view() const;
  // Returns the title of the Sharing Hub bubble.
  std::u16string GetWindowTitle() const;
  // Returns the current profile.
  Profile* GetProfile() const;
  // Returns true if the omnibox icon should be shown.
  bool ShouldOfferOmniboxIcon();

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
#endif

 protected:
  explicit SharingHubBubbleController(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<SharingHubBubbleController>;

  SharingHubModel* GetSharingHubModel();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  sharesheet::SharesheetService* GetSharesheetService();
  void ShowSharesheet(views::Button* highlighted_button);
  void CloseSharesheet();
  void OnShareDelivered(sharesheet::SharesheetResult result);
  void OnSharesheetClosed(views::Widget::ClosedReason reason);

  void DeselectIcon();

  views::ViewTracker highlighted_button_tracker_;
  sharesheet::SharesheetService* sharesheet_service_ = nullptr;
  gfx::NativeWindow web_contents_containing_window_ = nullptr;
  bool bubble_showing_ = false;
#endif

  // Weak reference. Will be nullptr if no bubble is currently shown.
  raw_ptr<SharingHubBubbleView> sharing_hub_bubble_view_ = nullptr;
  // Cached reference to the model.
  raw_ptr<SharingHubModel> sharing_hub_model_ = nullptr;

  base::RepeatingCallbackList<void(ui::ImageModel)>
      preview_image_changed_callbacks_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_H_
