// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_BUBBLE_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_BUBBLE_VIEW_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/share/share_attempt.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace sharing_hub {

class SharingHubBubbleController;
class SharingHubBubbleActionButton;
struct SharingHubAction;

// View component of the Sharing Hub bubble that allows users to share/save the
// current page. The sharing hub bubble also optionally contains a preview of
// the content being shared.
class SharingHubBubbleViewImpl : public SharingHubBubbleView,
                                 public LocationBarBubbleDelegateView {
  METADATA_HEADER(SharingHubBubbleViewImpl, LocationBarBubbleDelegateView)

 public:
  // Bubble will be anchored to |anchor_view|.
  SharingHubBubbleViewImpl(views::View* anchor_view,
                           share::ShareAttempt attempt,
                           SharingHubBubbleController* controller);

  SharingHubBubbleViewImpl(const SharingHubBubbleViewImpl&) = delete;
  SharingHubBubbleViewImpl& operator=(const SharingHubBubbleViewImpl&) = delete;

  ~SharingHubBubbleViewImpl() override;

  // SharingHubBubbleView:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void OnThemeChanged() override;

  // Public for testing.
  void OnActionSelected(SharingHubBubbleActionButton* button);

 private:
  // views::BubbleDialogDelegateView:
  void Init() override;

  // Creates the scroll view.
  void CreateScrollView();

  // Populates the scroll view containing sharing actions.
  void PopulateScrollView(
      const std::vector<SharingHubAction>& first_party_actions);

  // Callback when the window is about to close, either from external causes
  // (parent window closed) or because the web contents requested it.
  void OnWindowClosing();

  // A raw pointer is *not* safe here; the controller can be torn down before
  // the bubble during the window close path, since the bubble will be closed
  // asynchronously during browser window teardown but the controller will be
  // destroyed synchronously.
  base::WeakPtr<SharingHubBubbleController> controller_;

  // ScrollView containing the list of share/save actions.
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;

  // The share attempt this bubble was opened for.
  const share::ShareAttempt attempt_;
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_BUBBLE_VIEW_IMPL_H_
