// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_SCRIM_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_SCRIM_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;
class BrowserView;

namespace split_tabs {
class SplitTabScrimDelegate;

// Coordinates the split tab scrim to show and hide.
class SplitTabScrimController : public OmniboxTabHelper::Observer,
                                public ChipController::Observer,
                                public views::WidgetObserver {
 public:
  explicit SplitTabScrimController(BrowserView* browser_view);
  ~SplitTabScrimController() override;

  bool ShouldShowScrim();

  // OmniboxTabHelper::Observer:
  void OnOmniboxFocusChanged(OmniboxFocusState state,
                             OmniboxFocusChangeReason reason) override;
  void OnOmniboxInputStateChanged() override {}
  void OnOmniboxInputInProgress(bool in_progress) override {}
  void OnOmniboxPopupVisibilityChanged(bool popup_is_open) override {}

  // ChipController::Observer:
  void OnPermissionPromptShown() override;
  void OnPermissionPromptHidden() override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroyed(views::Widget* widget) override;

 private:
  void OnActiveTabChange(BrowserWindowInterface* browser_window_interface);
  void OnTabWillDetach(tabs::TabInterface* tab_interface,
                       tabs::TabInterface::DetachReason reason);
  void OnPageInfoBubbleCreated(content::WebContents* web_contents,
                               views::Widget* bubble_widget);
  void UpdateScrimVisibility();

  bool is_permission_prompt_showing_ = false;
  bool is_page_info_bubble_showing_ = false;
  base::CallbackListSubscription active_tab_change_subscription_;
  base::CallbackListSubscription tab_will_detach_subscription_;
  base::CallbackListSubscription page_info_bubble_created_subscription_;
  base::ScopedObservation<OmniboxTabHelper, OmniboxTabHelper::Observer>
      omnibox_tab_helper_observation_{this};
  base::ScopedObservation<ChipController, ChipController::Observer>
      chip_controller_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      page_info_bubble_observation_{this};
  std::unique_ptr<SplitTabScrimDelegate> split_tab_scrim_delegate_;
  raw_ptr<BrowserWindowInterface> browser_window_interface_;
};
}  // namespace split_tabs

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_SCRIM_CONTROLLER_H_
