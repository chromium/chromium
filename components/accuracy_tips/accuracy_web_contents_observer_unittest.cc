// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/accuracy_web_contents_observer.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/accuracy_tips/accuracy_service.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/accuracy_tips/features.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using AccuracyCheckCallback =
    accuracy_tips::AccuracyService::AccuracyCheckCallback;

namespace accuracy_tips {
namespace {
// Helpers to invoke callback on mocked function.
void ReturnNone(const GURL&, AccuracyCheckCallback callback) {
  std::move(callback).Run(AccuracyTipStatus::kNone);
}
void ReturnIsNewsRelated(const GURL&, AccuracyCheckCallback callback) {
  std::move(callback).Run(AccuracyTipStatus::kShowAccuracyTip);
}
}  // namespace

class MockAccuracyService : public AccuracyService {
 public:
  MockAccuracyService()
      : AccuracyService(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) {}
  MOCK_METHOD2(CheckAccuracyStatus, void(const GURL&, AccuracyCheckCallback));
  MOCK_METHOD1(MaybeShowAccuracyTip, void(content::WebContents*));

  bool IsSecureConnection(content::WebContents* web_contents) override {
    return web_contents->GetURL().SchemeIsCryptographic();
  }
};

class AccuracyWebContentsObserverTest
    : public content::RenderViewHostTestHarness {
 protected:
  AccuracyWebContentsObserverTest() = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    service_ = std::make_unique<testing::StrictMock<MockAccuracyService>>();
  }

  MockAccuracyService* service() { return service_.get(); }
  base::HistogramTester* tester() { return &tester_; }

 private:
  std::unique_ptr<testing::StrictMock<MockAccuracyService>> service_;
  base::HistogramTester tester_;
};

TEST_F(AccuracyWebContentsObserverTest, CheckServiceOnNavigationToRandomSite) {
  AccuracyWebContentsObserver::CreateForWebContents(web_contents(), service());
  GURL url("https://example.com");
  EXPECT_CALL(*service(), CheckAccuracyStatus(url, _))
      .WillOnce(Invoke(&ReturnNone));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  tester()->ExpectUniqueSample("Privacy.AccuracyTip.PageStatus",
                               AccuracyTipStatus::kNone, 1);
}

TEST_F(AccuracyWebContentsObserverTest,
       CheckServiceOnNavigationToRandomInsecureSite) {
  AccuracyWebContentsObserver::CreateForWebContents(web_contents(), service());
  GURL url("http://example.com");
  EXPECT_CALL(*service(), CheckAccuracyStatus(url, _))
      .WillOnce(Invoke(&ReturnNone));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
  tester()->ExpectUniqueSample("Privacy.AccuracyTip.PageStatus",
                               AccuracyTipStatus::kNone, 1);
}

TEST_F(AccuracyWebContentsObserverTest, CheckServiceOnNavigationToSiteInList) {
  AccuracyWebContentsObserver::CreateForWebContents(web_contents(), service());
  GURL url("https://accuracytip.com");
  EXPECT_CALL(*service(), CheckAccuracyStatus(url, _))
      .WillOnce(Invoke(&ReturnIsNewsRelated));
  EXPECT_CALL(*service(), MaybeShowAccuracyTip(web_contents()));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  tester()->ExpectUniqueSample("Privacy.AccuracyTip.PageStatus",
                               AccuracyTipStatus::kShowAccuracyTip, 1);
}

TEST_F(AccuracyWebContentsObserverTest,
       CheckServiceOnNavigationToInSecureSiteInList) {
  AccuracyWebContentsObserver::CreateForWebContents(web_contents(), service());
  GURL url("http://accuracytip.com");
  EXPECT_CALL(*service(), CheckAccuracyStatus(url, _))
      .WillOnce(Invoke(&ReturnIsNewsRelated));
  EXPECT_CALL(*service(), MaybeShowAccuracyTip(web_contents())).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  tester()->ExpectUniqueSample("Privacy.AccuracyTip.PageStatus",
                               AccuracyTipStatus::kNotSecure, 1);
}

TEST_F(AccuracyWebContentsObserverTest, CheckServiceAndNavigationBeforeResult) {
  AccuracyWebContentsObserver::CreateForWebContents(web_contents(), service());
  GURL example_url("https://example.com");
  GURL accuracy_tip_url("https://accuracytip.com");

  // Capture callback for first navigation.
  AccuracyCheckCallback callback;
  EXPECT_CALL(*service(), CheckAccuracyStatus(accuracy_tip_url, _))
      .WillOnce(Invoke([&](const GURL&, AccuracyCheckCallback cb) {
        callback = std::move(cb);
      }));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             accuracy_tip_url);
  Mock::VerifyAndClearExpectations(service());

  // Navigate to a different site.
  EXPECT_CALL(*service(), CheckAccuracyStatus(example_url, _))
      .WillOnce(Invoke(&ReturnNone));
  // Twice, once per each time a callback is ran.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             example_url);

  // Verify that there is no call to MaybeShowAccuracyTip if callback is invoked
  // after navigation to a different site.
  std::move(callback).Run(AccuracyTipStatus::kShowAccuracyTip);
  Mock::VerifyAndClearExpectations(service());

  tester()->ExpectUniqueSample("Privacy.AccuracyTip.PageStatus",
                               AccuracyTipStatus::kNone, 1);
}

TEST_F(AccuracyWebContentsObserverTest, CheckServiceAndDestroyBeforeResult) {
  AccuracyWebContentsObserver::CreateForWebContents(web_contents(), service());
  GURL url("https://accuracytip.com");
  AccuracyCheckCallback callback;
  EXPECT_CALL(*service(), CheckAccuracyStatus(url, _))
      .WillOnce(Invoke([&](const GURL&, AccuracyCheckCallback cb) {
        callback = std::move(cb);
      }));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  // Invoke callback after webcontents is destroyed.
  DeleteContents();
  std::move(callback).Run(AccuracyTipStatus::kShowAccuracyTip);

  tester()->ExpectTotalCount("Privacy.AccuracyTip.PageStatus", 0);
}

}  // namespace accuracy_tips
