// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_H_

#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "content/public/browser/web_contents_user_data.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace sharing_hub {

class SharingHubBubbleView;
struct SharingHubAction;

// Controller component of the Sharing Hub dialog bubble.
// Responsible for showing and hiding an owned bubble.
class SharingHubBubbleController
    : public content::WebContentsUserData<SharingHubBubbleController> {
 public:
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

  // Returns the list of Sharing Hub actions.
  virtual std::vector<SharingHubAction> GetActions() const;

  // Handles when the user clicks on a Sharing Hub action. If this is a first
  // party action, executes the appropriate browser command. If this is a third
  // party action, navigates to an external webpage.
  virtual void OnActionSelected(int command_id, bool is_first_party);
  // Handler for when the bubble is closed.
  void OnBubbleClosed();

 protected:
  SharingHubBubbleController();
  explicit SharingHubBubbleController(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<SharingHubBubbleController>;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void ShowSharesheet();
  void OnSharesheetShown(sharesheet::SharesheetResult result);
#endif

  // The web_contents associated with this controller.
  content::WebContents* web_contents_;
  // Weak reference. Will be nullptr if no bubble is currently shown.
  SharingHubBubbleView* sharing_hub_bubble_view_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SharingHubBubbleController);
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_H_
