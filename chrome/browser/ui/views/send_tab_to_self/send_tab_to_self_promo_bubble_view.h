// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

namespace send_tab_to_self {

// TODO(crbug.com/488252159): Move these classes to separate files.

// Shown when the user is signed in but has no other active target devices.
class SendTabToSelfNoTargetDeviceBubbleView : public SendTabToSelfBubbleView {
  METADATA_HEADER(SendTabToSelfNoTargetDeviceBubbleView,
                  SendTabToSelfBubbleView)

 public:
  SendTabToSelfNoTargetDeviceBubbleView(views::BubbleAnchor anchor,
                                        content::WebContents* web_contents);
  SendTabToSelfNoTargetDeviceBubbleView(
      const SendTabToSelfNoTargetDeviceBubbleView&) = delete;
  SendTabToSelfNoTargetDeviceBubbleView& operator=(
      const SendTabToSelfNoTargetDeviceBubbleView&) = delete;
  ~SendTabToSelfNoTargetDeviceBubbleView() override;

 private:
  // Private helper to construct the view hierarchy.
  void InitLayout();
};

// Shown when the user is signed out, offering a promotional sign-in flow.
class SendTabToSelfSignInPromoBubbleView : public SendTabToSelfBubbleView {
  METADATA_HEADER(SendTabToSelfSignInPromoBubbleView, SendTabToSelfBubbleView)

 public:
  SendTabToSelfSignInPromoBubbleView(views::BubbleAnchor anchor,
                                     content::WebContents* web_contents);
  SendTabToSelfSignInPromoBubbleView(
      const SendTabToSelfSignInPromoBubbleView&) = delete;
  SendTabToSelfSignInPromoBubbleView& operator=(
      const SendTabToSelfSignInPromoBubbleView&) = delete;
  ~SendTabToSelfSignInPromoBubbleView() override;

  // views::WidgetObserver:
  // Sets up header illustration or custom profile styling.
  void AddedToWidget() override;

  // views::BubbleDialogDelegateView:
  // Override to prevent the OK button from receiving initial focus on display.
  views::View* GetInitiallyFocusedView() override;

 private:
  // Constructs the basic, text-only layout for the sign-in promo.
  // Used when the enhanced UI is disabled.
  void InitBasicLayout();

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Constructs the modernized, enhanced layout for the sign-in promo.
  void InitEnhancedLayout();
#endif

  // Launches the Dice sign-in tab.
  void HandleSignInButtonClicked();

  // Returns true if the modernized/enhanced sign-in promo UI should be shown
  // instead of the legacy design.
  bool IsEnhancedUiEnabled() const;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_
