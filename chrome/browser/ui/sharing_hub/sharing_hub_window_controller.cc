// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sharing_hub/sharing_hub_window_controller.h"

#include <optional>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/share/share_attempt.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/sharing_hub/screenshot/screenshot_captured_bubble.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/action_id.h"
#include "ui/gfx/image/image.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"
#endif

namespace sharing_hub {

DEFINE_USER_DATA(SharingHubWindowController);

// static
SharingHubWindowController* SharingHubWindowController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

SharingHubWindowController::SharingHubWindowController(
    BrowserWindowInterface* browser)
    : browser_(*browser),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {}

SharingHubWindowController::~SharingHubWindowController() = default;

#if !BUILDFLAG(IS_CHROMEOS)
SharingHubBubbleView* SharingHubWindowController::ShowSharingHubBubble(
    share::ShareAttempt attempt) {
  views::BubbleAnchor anchor =
      ToolbarButtonProvider::From(&*browser_)->GetBubbleAnchor(std::nullopt);
  auto bubble = std::make_unique<SharingHubBubbleViewImpl>(
      anchor, attempt,
      SharingHubBubbleController::CreateOrGetFromWebContents(
          attempt.web_contents.get()));
  auto* bubble_ptr = bubble.get();
  bubble->SetHighlightedElement(SharingHubBubbleController::kIconElementId);

  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
  bubble_ptr->ShowForReason(SharingHubBubbleViewImpl::USER_GESTURE);

  return bubble_ptr;
}
#endif

ScreenshotCapturedBubble*
SharingHubWindowController::ShowScreenshotCapturedBubble(
    content::WebContents* contents,
    const gfx::Image& image) {
  views::BubbleAnchor anchor =
      ToolbarButtonProvider::From(&*browser_)->GetBubbleAnchor(std::nullopt);
  auto bubble = std::make_unique<ScreenshotCapturedBubble>(
      anchor, contents, image,
      Profile::FromBrowserContext(contents->GetBrowserContext()));
  auto* bubble_ptr = bubble.get();

  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
  bubble_ptr->ShowForReason(LocationBarBubbleDelegateView::USER_GESTURE);
  return bubble_ptr;
}

}  // namespace sharing_hub
