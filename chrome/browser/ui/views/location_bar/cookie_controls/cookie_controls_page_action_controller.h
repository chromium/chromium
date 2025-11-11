// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_PAGE_ACTION_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_observer.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class Profile;
class ToolbarButtonProvider;

namespace content {
class WebContents;
}  // namespace content

namespace content_settings {
class CookieControlsController;
}  // namespace content_settings

// `CookieControlsPageActionController` is responsible for managing the cookie
// controls page action, including logic for showing/hiding and executing the
// page action.
class CookieControlsPageActionController
    : public content_settings::CookieControlsObserver,
      public page_actions::PageActionObserver {
 public:
  DECLARE_USER_DATA(CookieControlsPageActionController);

  // An interface for interacting with the Cookie Controls bubble.
  class BubbleDelegate {
   public:
    virtual ~BubbleDelegate() = default;
    virtual bool HasBubble() = 0;
    virtual void ShowBubble(
        ToolbarButtonProvider* toolbar_button_provider,
        content::WebContents* web_contents,
        content_settings::CookieControlsController* controller) = 0;
    virtual base::CallbackListSubscription RegisterBubbleClosingCallback(
        base::RepeatingClosure callback) = 0;
  };

  CookieControlsPageActionController(
      tabs::TabInterface& tab_interface,
      Profile& profile,
      page_actions::PageActionController& page_action_controller);

  CookieControlsPageActionController(
      const CookieControlsPageActionController&) = delete;
  CookieControlsPageActionController& operator=(
      const CookieControlsPageActionController&) = delete;
  ~CookieControlsPageActionController() override;

  static CookieControlsPageActionController* From(tabs::TabInterface& tab);

  void Init();

  // PageActionObserver
  void OnPageActionChipShown(
      const page_actions::PageActionState& page_action) override;

  // CookieControlsObserver:
  void OnCookieControlsIconStatusChanged(
      bool icon_visible,
      CookieControlsState controls_state,
      CookieBlocking3pcdStatus blocking_status,
      bool should_highlight) override;
  void OnFinishedPageReloadWithChangedSettings() override;

  void ExecutePageAction(ToolbarButtonProvider* toolbar_button_provider);

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

  // Updates the icon's visibility.
  void UpdateIconVisibility();

  std::u16string GetLabelForState() const;
  bool ShouldShowIcon() const;
  bool IsManagedIPHActive() const;
  void OnShowPromoResult(user_education::FeaturePromoResult result);
  void OnIPHClosed();
  void OnBubbleClosed();
  void MaybeShowIPH(BrowserUserEducationInterface& user_education);

  const raw_ref<tabs::TabInterface> tab_;
  const raw_ref<page_actions::PageActionController> page_action_controller_;
  std::unique_ptr<content_settings::CookieControlsController>
      cookie_controls_controller_;
  std::unique_ptr<BubbleDelegate> bubble_delegate_;

  // Tracks when an IPH is showing, ensuring the icon is highlighted.
  std::optional<page_actions::ScopedPageActionActivity> iph_activity_ =
      std::nullopt;

  CookieControlsIconStatus icon_status_;

  base::CallbackListSubscription will_discard_contents_subscription_;
  base::CallbackListSubscription tab_deactivation_subscription_;
  base::CallbackListSubscription tab_will_detach_subscription_;
  base::CallbackListSubscription bubble_will_close_subscription_;

  base::ScopedObservation<content_settings::CookieControlsController,
                          content_settings::CookieControlsObserver>
      controller_observation_{this};

  // Timer used to collapse from the chip state after some time.
  base::OneShotTimer hide_chip_timer_;

  ui::ScopedUnownedUserData<CookieControlsPageActionController>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<CookieControlsPageActionController> weak_ptr_factory_{
      this};
};
#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_PAGE_ACTION_CONTROLLER_H_
