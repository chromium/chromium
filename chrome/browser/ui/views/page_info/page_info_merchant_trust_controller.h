// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "components/page_info/core/page_info_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/events/event.h"

namespace page_info {
class MerchantTrustService;
enum class MerchantTrustInteraction;
}  // namespace page_info

class PageInfoMerchantTrustContentView;

class PageInfoMerchantTrustController : public content::WebContentsObserver {
 public:
  PageInfoMerchantTrustController(
      PageInfoMerchantTrustContentView* content_view,
      content::WebContents* web_contents);
  ~PageInfoMerchantTrustController() override;

  void MerchantBubbleOpened(page_info::MerchantBubbleOpenReferrer referrer);
  void MerchantBubbleClosed();

 private:
  void OnMerchantTrustDataFetched(
      const GURL& url,
      std::optional<page_info::MerchantData> merchant_data);
  void LearnMoreLinkPressed(const ui::Event& event);
  void ViewReviewsPressed();
  void HatsButtonPressed();
  void OnSurveyLoaded();
  void OnSurveyFailed();
  void InitCallbacks();
  void RecordInteractionPref();
  void RecordInteraction(page_info::MerchantTrustInteraction interaction);

  raw_ptr<PageInfoMerchantTrustContentView> content_view_;
  page_info::MerchantData merchant_data_;

  base::CallbackListSubscription learn_more_link_callback_;
  base::CallbackListSubscription view_reviews_button_callback_;
  base::CallbackListSubscription hats_button_callback_;

  base::OneShotTimer interaction_timer_;
  raw_ptr<page_info::MerchantTrustService> service_;
  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();

  base::WeakPtrFactory<PageInfoMerchantTrustController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTROLLER_H_
