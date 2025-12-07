// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_media_link_handler.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/media_session.h"
#include "content/public/test/mock_media_session.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace lens {

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;

// Subclass for `LensMediaLinkHandler` that lets us provide the `MediaSession`
// object directly, since trying to get MediaSession::FromWebContents() to
// return a mock is basically impossible without some fairly big changes.
class LensMediaLinkHandlerForTest : public lens::LensMediaLinkHandler {
 public:
  LensMediaLinkHandlerForTest(content::WebContents* web_contents,
                              content::MediaSession* media_session)
      : LensMediaLinkHandler(web_contents), media_session_(media_session) {}

  content::MediaSession* GetMediaSessionIfExists() override {
    return media_session_;
  }

  void clear_media_session() { media_session_ = nullptr; }

 private:
  raw_ptr<content::MediaSession> media_session_ = nullptr;
};

class LensMediaLinkHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  LensMediaLinkHandlerTest() = default;
  ~LensMediaLinkHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    lens_media_link_handler_ = std::make_unique<LensMediaLinkHandlerForTest>(
        web_contents(), &mock_media_session_);
  }

  void TearDown() override {
    lens_media_link_handler_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::MockMediaSession& mock_media_session() {
    return mock_media_session_;
  }

  LensMediaLinkHandlerForTest* lens_media_link_handler() {
    return lens_media_link_handler_.get();
  }

  void CreateAndAttachRoutedSubframe(const GURL& subframe_url) {
    // Set up a subframe with an embed URL.
    content::RenderFrameHost* subframe =
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            subframe_url, content::RenderFrameHostTester::For(
                              web_contents()->GetPrimaryMainFrame())
                              ->AppendChild("subframe"));

    ON_CALL(mock_media_session(), GetRoutedFrame)
        .WillByDefault(Return(subframe));
  }

 protected:
  content::MockMediaSession mock_media_session_;
  std::unique_ptr<LensMediaLinkHandlerForTest> lens_media_link_handler_;
};

TEST_F(LensMediaLinkHandlerTest, DifferentHostsReturnsFalse) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());

  // Case 1: Committed is example.com, target is youtube.com
  web_contents_tester->NavigateAndCommit(GURL("https://www.example.com/page"));
  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123")));

  // Case 2: Different subdomains of YouTube are considered different hosts.
  web_contents_tester->NavigateAndCommit(
      GURL("https://m.youtube.com/watch?v=video123"));
  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30s")));
}

TEST_F(LensMediaLinkHandlerTest, SameOriginNoHelperReturnsFalse) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL("https://www.example.com/page"));

  // example.com is not in g_origin_helpers
  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.example.com/another_page")));
}

TEST_F(LensMediaLinkHandlerTest, EmbeddedVideo_SuccessWithWatchTarget) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  CreateAndAttachRoutedSubframe(GURL("https://www.youtube.com/embed/video123"));

  EXPECT_CALL(mock_media_session(), SeekTo(Eq(base::Seconds(30)))).Times(1);

  EXPECT_TRUE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30s")));
}

TEST_F(LensMediaLinkHandlerTest, EmbeddedVideo_SuccessWithEmbedTarget) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  CreateAndAttachRoutedSubframe(GURL("https://www.youtube.com/embed/video123"));

  EXPECT_CALL(mock_media_session(), SeekTo(Eq(base::Seconds(45)))).Times(1);

  EXPECT_TRUE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/embed/video123?t=45s")));
}

TEST_F(LensMediaLinkHandlerTest, EmbeddedVideo_MismatchedVideoId) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  CreateAndAttachRoutedSubframe(GURL("https://www.youtube.com/embed/video123"));

  EXPECT_CALL(mock_media_session(), SeekTo(_)).Times(0);

  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=DIFFERENT&t=30s")));
}

TEST_F(LensMediaLinkHandlerTest, EmbeddedVideo_NoTimeParam) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  CreateAndAttachRoutedSubframe(GURL("https://www.youtube.com/embed/video123"));

  EXPECT_CALL(mock_media_session(), SeekTo(_)).Times(0);

  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123")));
}

TEST_F(LensMediaLinkHandlerTest, EmbeddedVideo_NoRoutedFrame) {
  ON_CALL(mock_media_session(), GetRoutedFrame).WillByDefault(Return(nullptr));

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com/"));

  EXPECT_CALL(mock_media_session(), SeekTo(_)).Times(0);

  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30s")));
}

TEST_F(LensMediaLinkHandlerTest, MainTabVideo_DifferentSchemeSucceeds) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("http://www.youtube.com/watch?v=video123"));

  EXPECT_CALL(mock_media_session(), SeekTo(Eq(base::Seconds(30)))).Times(1);

  EXPECT_TRUE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30s")));
}

TEST_F(LensMediaLinkHandlerTest, MainTabVideo_MobileYouTubeIsNotHandled) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://m.youtube.com/watch?v=video123"));

  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://m.youtube.com/watch?v=video123&t=30s")));
}

TEST_F(LensMediaLinkHandlerTest, MainTabVideo_NoVideoIdInCommittedUrl) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  // Committed URL without 'v=' parameter
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/feed/subscriptions"));

  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30s")));
}

TEST_F(LensMediaLinkHandlerTest, MainTabVideo_NoVideoIdInTargetUrl) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Target URL without 'v=' parameter
  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/feed/subscriptions?t=30s")));
}

TEST_F(LensMediaLinkHandlerTest, MainTabVideo_DifferentVideoIds) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=different&t=30s")));
}

TEST_F(LensMediaLinkHandlerTest, MainTabVideo_EmptyVideoId) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());

  // Committed URL with empty 'v='
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v="));
  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30s")));

  // Target URL with empty 'v='
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));
  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=&t=30s")));
}

TEST_F(LensMediaLinkHandlerTest, MainTabVideo_NoTimeParam) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Target URL without 't=' parameter
  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&q=test")));
}

TEST_F(LensMediaLinkHandlerTest, MainTabVideo_EmptyTimeParam) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Target URL with empty 't=' parameter
  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=")));
}

TEST_F(LensMediaLinkHandlerTest, MainTabVideo_NonIntegerTimeParam) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Target URL with non-integer 't=' parameter
  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=abc")));
}

TEST_F(LensMediaLinkHandlerTest, MainTabVideo_NoMediaSession) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // No MediaSession attached, so MediaSession::GetIfExists will return nullptr.
  lens_media_link_handler()->clear_media_session();
  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=60s")));
}

TEST_F(LensMediaLinkHandlerTest, MainTabVideo_WithMediaSession) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Attach a mock media session and expect SeekTo to be called.
  EXPECT_CALL(mock_media_session(), SeekTo(Eq(base::Seconds(30)))).Times(1);

  EXPECT_TRUE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=30s")));
}

TEST_F(LensMediaLinkHandlerTest, MainTabVideo_WithMediaSession_ZeroTime) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  EXPECT_CALL(mock_media_session(), SeekTo(Eq(base::Seconds(0)))).Times(1);

  EXPECT_TRUE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=0s")));
}

TEST_F(LensMediaLinkHandlerTest,
       MainTabVideo_WithMediaSession_NegativeTimeParam) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Expect SeekTo to NOT be called because base::StringToUint fails for
  // negative numbers.
  EXPECT_CALL(mock_media_session(), SeekTo(_)).Times(0);

  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=-10s")));
}

TEST_F(LensMediaLinkHandlerTest,
       MainTabVideo_WithMediaSession_FractionalTimeParam) {
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(
      GURL("https://www.youtube.com/watch?v=video123"));

  // Expect SeekTo to NOT be called because base::StringToUint fails for
  // non-integers, which also don't work for `t=` parameters.
  EXPECT_CALL(mock_media_session(), SeekTo(_)).Times(0);

  EXPECT_FALSE(lens_media_link_handler()->MaybeReplaceNavigation(
      GURL("https://www.youtube.com/watch?v=video123&t=10.5s")));
}

}  // namespace
}  // namespace lens
