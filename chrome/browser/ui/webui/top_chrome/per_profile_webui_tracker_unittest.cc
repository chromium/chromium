// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/per_profile_webui_tracker.h"

#include "base/scoped_observation.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
constexpr char kWebUIUrl1[] = "chrome://example";
constexpr char kWebUIUrl2[] = "chrome://example2";
}  // namespace

class PerProfileWebUITrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  PerProfileWebUITrackerTest() : tracker_(PerProfileWebUITracker::Create()) {}
  ~PerProfileWebUITrackerTest() override = default;
  PerProfileWebUITrackerTest(const PerProfileWebUITrackerTest&) = delete;
  PerProfileWebUITrackerTest& operator=(const PerProfileWebUITrackerTest&) =
      delete;

  PerProfileWebUITracker* tracker() { return tracker_.get(); }

 private:
  std::unique_ptr<PerProfileWebUITracker> tracker_;
};

TEST_F(PerProfileWebUITrackerTest, Basic) {
  std::unique_ptr<content::WebContents> web_contents_1, web_contents_2;
  web_contents_1 = CreateTestWebContents();
  web_contents_2 = CreateTestWebContents();
  content::WebContentsTester::For(web_contents_1.get())
      ->NavigateAndCommit(GURL(kWebUIUrl1));
  content::WebContentsTester::For(web_contents_2.get())
      ->NavigateAndCommit(GURL(kWebUIUrl2));

  ASSERT_EQ(web_contents_1->GetBrowserContext(), profile());

  tracker()->AddWebContents(web_contents_1.get());
  EXPECT_TRUE(tracker()->ProfileHasWebUI(profile(), kWebUIUrl1));
  // web_contents_2 is not tracked, therefore ProfileHasWebUI() returns false
  // for kWebUIUrl2.
  EXPECT_FALSE(tracker()->ProfileHasWebUI(profile(), kWebUIUrl2));

  tracker()->AddWebContents(web_contents_2.get());
  EXPECT_TRUE(tracker()->ProfileHasWebUI(profile(), kWebUIUrl2));

  // Deleting web_contents_1 causes kWebUIUrl1 to be no longer present, while
  // kWebUIUrl2 is still present.
  web_contents_1.reset();
  EXPECT_FALSE(tracker()->ProfileHasWebUI(profile(), kWebUIUrl1));
  EXPECT_TRUE(tracker()->ProfileHasWebUI(profile(), kWebUIUrl2));

  web_contents_2.reset();
  EXPECT_FALSE(tracker()->ProfileHasWebUI(profile(), kWebUIUrl1));
  EXPECT_FALSE(tracker()->ProfileHasWebUI(profile(), kWebUIUrl2));
}

// Tests that when a profile opens two WebUIs of the same URL, the tracker
// will indicate that the WebUI is still present after closing one of them.
TEST_F(PerProfileWebUITrackerTest, TwoWebUIsOfSameURL) {
  std::unique_ptr<content::WebContents> web_contents_1, web_contents_2;
  web_contents_1 = CreateTestWebContents();
  web_contents_2 = CreateTestWebContents();
  // Both WebContents navigate to kWebUIUrl1.
  content::WebContentsTester::For(web_contents_1.get())
      ->NavigateAndCommit(GURL(kWebUIUrl1));
  content::WebContentsTester::For(web_contents_2.get())
      ->NavigateAndCommit(GURL(kWebUIUrl1));

  ASSERT_EQ(web_contents_1->GetBrowserContext(), profile());

  tracker()->AddWebContents(web_contents_1.get());
  tracker()->AddWebContents(web_contents_2.get());
  EXPECT_TRUE(tracker()->ProfileHasWebUI(profile(), kWebUIUrl1));

  // Close one WebUI, the WebUI is still present under this profile.
  web_contents_1.reset();
  EXPECT_TRUE(tracker()->ProfileHasWebUI(profile(), kWebUIUrl1));

  // Close the other WebUI, no WebUI is present under this profile.
  web_contents_2.reset();
  EXPECT_FALSE(tracker()->ProfileHasWebUI(profile(), kWebUIUrl1));
}

// Tests that the tracker distinguishes different profiles.
TEST_F(PerProfileWebUITrackerTest, DifferentProfiles) {
  std::unique_ptr<TestingProfile> profile1 = CreateTestingProfile();
  std::unique_ptr<TestingProfile> profile2 = CreateTestingProfile();
  std::unique_ptr<content::WebContents> web_contents_1 =
      content::WebContentsTester::CreateTestWebContents(profile1.get(),
                                                        nullptr);
  std::unique_ptr<content::WebContents> web_contents_2 =
      content::WebContentsTester::CreateTestWebContents(profile2.get(),
                                                        nullptr);
  content::WebContentsTester::For(web_contents_1.get())
      ->NavigateAndCommit(GURL(kWebUIUrl1));
  content::WebContentsTester::For(web_contents_2.get())
      ->NavigateAndCommit(GURL(kWebUIUrl2));
  tracker()->AddWebContents(web_contents_1.get());
  tracker()->AddWebContents(web_contents_2.get());
  EXPECT_TRUE(tracker()->ProfileHasWebUI(profile1.get(), kWebUIUrl1));
  EXPECT_FALSE(tracker()->ProfileHasWebUI(profile1.get(), kWebUIUrl2));
  EXPECT_FALSE(tracker()->ProfileHasWebUI(profile2.get(), kWebUIUrl1));
  EXPECT_TRUE(tracker()->ProfileHasWebUI(profile2.get(), kWebUIUrl2));
}

// Tests that the tracker work as expected when a WebContents navigates from
// about:blank to a WebUI.
TEST_F(PerProfileWebUITrackerTest, Navigation) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  ASSERT_EQ(web_contents->GetBrowserContext(), profile());

  // The WebContents is not yet navigated.
  tracker()->AddWebContents(web_contents.get());
  EXPECT_FALSE(tracker()->ProfileHasWebUI(profile(), kWebUIUrl1));

  // Navigate to a WebUI.
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL(kWebUIUrl1));
  EXPECT_TRUE(tracker()->ProfileHasWebUI(profile(), kWebUIUrl1));

  // Navigate away.
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("about:blank"));
  EXPECT_FALSE(tracker()->ProfileHasWebUI(profile(), kWebUIUrl1));
}

class MockTrackerObserver : public PerProfileWebUITracker::Observer {
 public:
  MOCK_METHOD(void,
              OnWebContentsDestroyed,
              (content::WebContents*),
              (override));
  MOCK_METHOD(void,
              OnWebContentsPrimaryPageChanged,
              (content::WebContents*),
              (override));
};

// Tests that the observer of tracker is notified of WebContents destroy.
TEST_F(PerProfileWebUITrackerTest, Observer) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  ASSERT_EQ(web_contents->GetBrowserContext(), profile());
  tracker()->AddWebContents(web_contents.get());

  MockTrackerObserver mock_observer;
  base::ScopedObservation<PerProfileWebUITracker,
                          PerProfileWebUITracker::Observer>
      observation{&mock_observer};
  EXPECT_CALL(mock_observer, OnWebContentsDestroyed(web_contents.get()));
  observation.Observe(tracker());
  web_contents.reset();
}
