// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_PAGE_ACTION_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/page_action/page_action_observer.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/tabs/public/tab_interface.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

// `CookieControlsPageActionController` is responsible for managing the cookie
// controls page action, including logic for showing/hiding and executing the
// page action.
class CookieControlsPageActionController
    : public content_settings::CookieControlsObserver {
 public:
  // An interface for fetching relevant Cookie Controls bubble state.
  class BubbleDelegate {
   public:
    virtual ~BubbleDelegate() = default;
    virtual bool IsReloading() = 0;
    virtual bool HasBubble() = 0;
  };

  CookieControlsPageActionController(
      tabs::TabInterface& tab_interface,
      page_actions::PageActionController& page_action_controller);

  CookieControlsPageActionController(
      const CookieControlsPageActionController&) = delete;
  CookieControlsPageActionController& operator=(
      const CookieControlsPageActionController&) = delete;
  ~CookieControlsPageActionController() override;

  // CookieControlsObserver:
  void OnCookieControlsIconStatusChanged(
      bool icon_visible,
      CookieControlsState controls_state,
      CookieBlocking3pcdStatus blocking_status,
      bool should_highlight) override;
  void OnFinishedPageReloadWithChangedSettings() override;

  void set_bubble_delegate_for_testing(
      std::unique_ptr<BubbleDelegate> delegate) {
    bubble_delegate_ = std::move(delegate);
  }

 private:
  // Encapsulates values provided by `OnCookieControlsIconStatusChanged`.
  struct CookieControlsIconStatus {
    bool icon_visible;
    CookieControlsState controls_state;
    CookieBlocking3pcdStatus blocking_status;
    bool should_highlight;
  };

  void UpdatePageActionIcon(bool from_page_reload);

  const raw_ref<page_actions::PageActionController> page_action_controller_;
  std::unique_ptr<BubbleDelegate> bubble_delegate_;

  CookieControlsIconStatus icon_status_;

  base::CallbackListSubscription tab_insert_subscription_;
};
#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_PAGE_ACTION_CONTROLLER_H_
