// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_PAGE_ACTION_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

// `CookieControlsPageActionController` is responsible for managing the cookie
// controls page action, including logic for showing/hiding and executing the
// page action.
class CookieControlsPageActionController
    : public content_settings::CookieControlsObserver {
 public:
  explicit CookieControlsPageActionController(
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

 private:
  // Encapsulates values provided by `OnCookieControlsIconStatusChanged`.
  struct CookieControlsIconStatus {
    bool icon_visible;
    CookieControlsState controls_state;
    CookieBlocking3pcdStatus blocking_status;
    bool should_highlight;
  };

  void UpdatePageActionIcon(const CookieControlsIconStatus& icon_status);

  const raw_ref<page_actions::PageActionController> page_action_controller_;
};
#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_PAGE_ACTION_CONTROLLER_H_
