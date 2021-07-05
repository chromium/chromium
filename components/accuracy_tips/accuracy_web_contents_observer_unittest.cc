// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/accuracy_web_contents_observer.h"

#include <memory>

#include "base/test/bind.h"
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
using AccuracyCheckCallback =
    accuracy_tips::AccuracyService::AccuracyCheckCallback;

namespace accuracy_tips {
namespace {
// Helpers to invoke callback on mocked function.
void ReturnNone(const GURL&, AccuracyCheckCallback callback) {
  std::move(callback).Run(AccuracyTipStatus::kNone);
}
void ReturnIsMisinformation(const GURL&, AccuracyCheckCallback callback) {
  std::move(callback).Run(AccuracyTipStatus::kShowAccuracyTip);
}
}  // namespace

class MockAccuracyService : public AccuracyService {
 public:
  MockAccuracyService() : AccuracyService(nullptr, nullptr, nullptr, nullptr) {}
  MOCK_METHOD2(CheckAccuracyStatus, void(const GURL&, AccuracyCheckCallback));
  MOCK_METHOD1(MaybeShowAccuracyTip, void(content::WebContents*));
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

 private:
  std::unique_ptr<testing::StrictMock<MockAccuracyService>> service_;
};

TEST_F(AccuracyWebContentsObserverTest, CheckServiceOnNavigationToRandomSite) {
  AccuracyWebContentsObserver::CreateForWebContents(web_contents(), service());
  EXPECT_CALL(*service(), CheckAccuracyStatus(GURL("https://example.com"), _))
      .WillOnce(Invoke(&ReturnNone));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com"));
}

TEST_F(AccuracyWebContentsObserverTest, CheckServiceOnNavigationToBadSite) {
  AccuracyWebContentsObserver::CreateForWebContents(web_contents(), service());
  EXPECT_CALL(*service(), CheckAccuracyStatus(GURL("https://badurl.com"), _))
      .WillOnce(Invoke(&ReturnIsMisinformation));
  EXPECT_CALL(*service(), MaybeShowAccuracyTip(web_contents()));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://badurl.com"));
}

TEST_F(AccuracyWebContentsObserverTest, CheckServiceAndNavigationBeforeResult) {
  AccuracyWebContentsObserver::CreateForWebContents(web_contents(), service());
  // Capture callback for first navigation.
  AccuracyCheckCallback callback;
  EXPECT_CALL(*service(), CheckAccuracyStatus(GURL("https://badurl.com"), _))
      .WillOnce(Invoke([&](const GURL&, AccuracyCheckCallback cb) {
        callback = std::move(cb);
      }));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://badurl.com"));
  Mock::VerifyAndClearExpectations(service());

  // Navigate to a different site.
  EXPECT_CALL(*service(), CheckAccuracyStatus(GURL("https://example.com"), _))
      .WillOnce(Invoke(&ReturnNone));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com"));

  // Verify that there is no call to MaybeShowAccuracyTip if callback is invoked
  // after navigation to a different site.
  std::move(callback).Run(AccuracyTipStatus::kShowAccuracyTip);
  Mock::VerifyAndClearExpectations(service());
}

TEST_F(AccuracyWebContentsObserverTest, CheckServiceAndDestroyBeforeResult) {
  AccuracyWebContentsObserver::CreateForWebContents(web_contents(), service());
  AccuracyCheckCallback callback;
  EXPECT_CALL(*service(), CheckAccuracyStatus(GURL("https://badurl.com"), _))
      .WillOnce(Invoke([&](const GURL&, AccuracyCheckCallback cb) {
        callback = std::move(cb);
      }));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://badurl.com"));

  // Invoke callback after webcontents is destroyed.
  DeleteContents();
  std::move(callback).Run(AccuracyTipStatus::kShowAccuracyTip);
}

}  // namespace accuracy_tips