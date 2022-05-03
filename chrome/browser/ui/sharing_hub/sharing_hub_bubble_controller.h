// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_H_

namespace content {
class WebContents;
}  // namespace content

namespace sharing_hub {

class SharingHubBubbleView;

// Interface for the controller component of the sharing dialog bubble. Controls
// the Sharing Hub (Windows/Mac/Linux) or the Sharesheet (CrOS) depending on
// platform.
// Responsible for showing and hiding an associated dialog bubble.
class SharingHubBubbleController {
 public:
  static SharingHubBubbleController* CreateOrGetFromWebContents(
      content::WebContents* web_contents);

  // Hides the sharing bubble.
  virtual void HideBubble() = 0;
  // Displays the sharing bubble.
  virtual void ShowBubble() = 0;

  // Returns nullptr if no bubble is currently shown.
  virtual SharingHubBubbleView* sharing_hub_bubble_view() const = 0;
  // Returns true if the omnibox icon should be shown.
  virtual bool ShouldOfferOmniboxIcon() = 0;
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_INTERFACE_H_
