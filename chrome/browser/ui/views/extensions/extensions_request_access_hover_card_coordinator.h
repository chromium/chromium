// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_HOVER_CARD_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_HOVER_CARD_COORDINATOR_H_

#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_id.h"
#include "ui/views/view_tracker.h"

namespace content {
class WebContents;
}  // namespace content

class ExtensionsContainer;

// Handles the lifetime and showing/hidden state of the request access button
// hover card.
class ExtensionsRequestAccessHoverCardCoordinator {
 public:
  ExtensionsRequestAccessHoverCardCoordinator() = default;
  ExtensionsRequestAccessHoverCardCoordinator(
      const ExtensionsRequestAccessHoverCardCoordinator&) = delete;
  const ExtensionsRequestAccessHoverCardCoordinator& operator=(
      const ExtensionsRequestAccessHoverCardCoordinator&) = delete;
  ~ExtensionsRequestAccessHoverCardCoordinator() = default;

  // Creates and shows the request access button bubble.
  void ShowBubble(content::WebContents* web_contents,
                  views::View* anchor_view,
                  ExtensionsContainer* extensions_container,
                  std::vector<extensions::ExtensionId>& extension_ids);

  // Hides the currently-showing request access button bubble, if any exists.
  void HideBubble();

  // Returns whether there is a currently a request access button bubble
  // showing.
  bool IsShowing() const;

 private:
  views::ViewTracker bubble_tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_HOVER_CARD_COORDINATOR_H_
