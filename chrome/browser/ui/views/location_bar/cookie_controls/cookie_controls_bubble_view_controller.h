// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "components/favicon_base/favicon_types.h"
#include "url/gurl.h"

class CookieControlsBubbleView;

class CookieControlsBubbleViewController
    : public content_settings::CookieControlsObserver {
 public:
  CookieControlsBubbleViewController(
      CookieControlsBubbleView* bubble_view,
      content_settings::CookieControlsController* controller,
      content::WebContents* web_contents);
  ~CookieControlsBubbleViewController() override;

  explicit CookieControlsBubbleViewController(
      content_settings::CookieControlsController* controller);

  // CookieControlsObserver:
  void OnStatusChanged(CookieControlsStatus status,
                       CookieControlsEnforcement enforcement,
                       base::Time expiration) override;
  void OnSitesCountChanged(int allowed_third_party_sites_count,
                           int blocked_third_party_sites_count) override;
  void OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel level) override;
  void OnFinishedPageReloadWithChangedSettings() override;

  void SetSubjectUrlNameForTesting(const std::u16string& name);

 private:
  friend class CookieControlsBubbleViewBrowserTest;

  void SetCallbacks();
  void OnUserClosedContentView();
  void OnToggleButtonPressed(bool new_value);
  void OnFeedbackButtonPressed();

  void OnFaviconFetched(const favicon_base::FaviconImageResult& result) const;

  void ApplyThirdPartyCookiesAllowedState(base::Time expiration);
  void ApplyThirdPartyCookiesBlockedState();

  [[nodiscard]] std::unique_ptr<views::View> InitReloadingView(
      content::WebContents* web_contents);

  void FetchFaviconFrom(content::WebContents* web_contents);

  std::u16string GetSubjectUrlName(content::WebContents* web_contents) const;

  // The most recent status provided by the CookieControlsController. Cached
  // so that updates to site counts can use the appropriate label.
  CookieControlsStatus latest_status_ = CookieControlsStatus::kUninitialized;

  raw_ptr<CookieControlsBubbleView> bubble_view_ = nullptr;

  // Used for favicon loading tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

  base::CallbackListSubscription on_user_closed_content_view_callback_;
  base::CallbackListSubscription toggle_button_callback_;
  base::CallbackListSubscription feedback_button_callback_;
  base::WeakPtr<content_settings::CookieControlsController> controller_;
  base::WeakPtr<content::WebContents> web_contents_;
  base::ScopedObservation<content_settings::CookieControlsController,
                          content_settings::CookieControlsObserver>
      controller_observation_{this};

  // Testing override for GetSubjectUrlName().
  absl::optional<std::u16string> subject_url_name_for_testing_;

  base::WeakPtrFactory<CookieControlsBubbleViewController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_CONTROLLER_H_
