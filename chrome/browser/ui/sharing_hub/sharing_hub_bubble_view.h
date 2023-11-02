// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_VIEW_H_

namespace sharing_hub {

// Interface to display the Sharing hub bubble.
// This object is responsible for its own lifetime.
class SharingHubBubbleView {
 public:
  virtual ~SharingHubBubbleView() = default;

  // Closes the bubble and prevents future calls into the controller.
  virtual void Hide() = 0;
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_VIEW_H_
