// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/interaction/feature_engagement_initialized_observer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "content/public/test/browser_test.h"

namespace {
DEFINE_LOCAL_REQUIRED_NOTICE_IDENTIFIER(kNoticeId);
}

class ProductMessagingControllerUiTest : public InteractiveBrowserTest {
 public:
  ProductMessagingControllerUiTest() {
    feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHDesktopTabGroupsNewGroupFeature});
  }

  ~ProductMessagingControllerUiTest() override = default;

  user_education::ProductMessagingController& GetProductMessagingController() {
    return UserEducationServiceFactory::GetForBrowserContext(
               browser()->profile())
        ->product_messaging_controller();
  }

  void TearDownOnMainThread() override {
    notice_handle_.Release();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  auto QueueNotice() {
    return Do([this]() {
      GetProductMessagingController().QueueRequiredNotice(
          kNoticeId,
          base::BindOnce(&ProductMessagingControllerUiTest::OnNoticeShown,
                         base::Unretained(this)));
    });
  }

  auto CheckShowPromo(user_education::FeaturePromoResult expected_result) {
    std::ostringstream oss;
    oss << "CheckShowPromo(" << expected_result << ")";
    return std::move(
        CheckResult(
            [this]() {
              return browser()->window()->MaybeShowFeaturePromo(
                  feature_engagement::kIPHDesktopTabGroupsNewGroupFeature);
            },
            expected_result)
            .SetDescription(oss.str().c_str()));
  }

  auto EnsureHandle() {
    return std::move(Check([this]() {
                       return !!notice_handle_;
                     }).SetDescription("EnsureHandle()"));
  }

  auto ReleaseHandle() {
    return std::move(Do([this]() {
                       notice_handle_.Release();
                     }).SetDescription("ReleaseHandle()"));
  }

  void OnNoticeShown(
      user_education::RequiredNoticePriorityHandle notice_handle) {
    notice_handle_ = std::move(notice_handle);
  }

  user_education::RequiredNoticePriorityHandle notice_handle_;

 private:
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProductMessagingControllerUiTest, NoticeBlocksIPH) {
  RunTestSequence(
      ObserveState(kFeatureEngagementInitializedState, browser()),
      WaitForState(kFeatureEngagementInitializedState, true), QueueNotice(),
      CheckShowPromo(user_education::FeaturePromoResult::kBlockedByUi),
      FlushEvents(), EnsureHandle(), ReleaseHandle(),
      CheckShowPromo(user_education::FeaturePromoResult::Success()));
}
