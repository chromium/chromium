// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapps_utils.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

namespace webapps {

namespace {

blink::Manifest GetValidManifest() {
  blink::Manifest::ImageResource icon;
  icon.type = u"image/png";
  icon.sizes.push_back(gfx::Size(144, 144));
  icon.src = GURL("http://example.com/icon.png");

  return std::move(blink::Manifest::Builder(GURL("http://example.com"),
                                            GURL("http://example.com"),
                                            GURL("http://example.com"))
                       .SetName(u"foo")
                       .SetShortName(u"bar")
                       .SetDisplay(blink::mojom::DisplayMode::kStandalone)
                       .SetIcons({icon}))
      .BuildChecked();
}

}  // anonymous namespace

TEST(WebappsUtilsTest, Compatible) {
  EXPECT_TRUE(
      WebappsUtils::AreWebManifestUrlsWebApkCompatible(GetValidManifest()));
}

TEST(WebappsUtilsTest, CompatibleURLHasNoPassword) {
  const GURL kStartUrlWithPassword(
      "http://answer:42@example.com/life/universe/and/everything");
  const GURL kScopeWithPassword("http://answer:42@example.com/");
  const GURL kIconWithPassword("http://answer:42@example.com/icon.png");

  blink::Manifest manifest_start_url =
      std::move(
          GetValidManifest().ToBuilder().SetStartUrl(kStartUrlWithPassword))
          .BuildChecked();
  EXPECT_FALSE(
      WebappsUtils::AreWebManifestUrlsWebApkCompatible(manifest_start_url));

  blink::Manifest manifest_scope =
      std::move(GetValidManifest().ToBuilder().SetScope(kScopeWithPassword))
          .BuildChecked();
  EXPECT_FALSE(
      WebappsUtils::AreWebManifestUrlsWebApkCompatible(manifest_scope));

  blink::Manifest::ImageResource icon;
  icon.src = kIconWithPassword;
  blink::Manifest manifest_icon =
      std::move(GetValidManifest().ToBuilder().SetIcons({icon})).BuildChecked();
  EXPECT_FALSE(WebappsUtils::AreWebManifestUrlsWebApkCompatible(manifest_icon));
}

class WebappsUtilsPruneHistoryTest : public content::RenderViewHostTestHarness {
 protected:
  // Returns the URL of every history entry, oldest first.
  std::vector<GURL> GetHistoryUrls() {
    std::vector<GURL> urls;
    content::NavigationController& controller = web_contents()->GetController();
    for (int i = 0; i < controller.GetEntryCount(); ++i) {
      urls.push_back(controller.GetEntryAtIndex(i)->GetURL());
    }
    return urls;
  }
};

// History: google, news, app/a, app/b (current). Scope: app/.
// Expect the two pages before the app to be removed.
TEST_F(WebappsUtilsPruneHistoryTest, PrunesEntriesBeforeScope) {
  const GURL kScope("https://example.com/app/");
  NavigateAndCommit(GURL("https://google.com/"));
  NavigateAndCommit(GURL("https://news.com/"));
  NavigateAndCommit(GURL("https://example.com/app/a"));
  NavigateAndCommit(GURL("https://example.com/app/b"));

  WebappsUtils::PrunePreScopeNavigationHistory(web_contents(), kScope);

  EXPECT_EQ(GetHistoryUrls(),
            (std::vector<GURL>{GURL("https://example.com/app/a"),
                               GURL("https://example.com/app/b")}));
}

// Every page is already in the app, so nothing should change.
TEST_F(WebappsUtilsPruneHistoryTest, NoOpWhenAllEntriesInScope) {
  const GURL kScope("https://example.com/app/");
  NavigateAndCommit(GURL("https://example.com/app/a"));
  NavigateAndCommit(GURL("https://example.com/app/b"));

  WebappsUtils::PrunePreScopeNavigationHistory(web_contents(), kScope);

  EXPECT_EQ(GetHistoryUrls(),
            (std::vector<GURL>{GURL("https://example.com/app/a"),
                               GURL("https://example.com/app/b")}));
}

// The current page is outside the app, so nothing should change.
TEST_F(WebappsUtilsPruneHistoryTest, NoOpWhenCurrentEntryOutOfScope) {
  const GURL kScope("https://example.com/app/");
  NavigateAndCommit(GURL("https://example.com/app/a"));
  NavigateAndCommit(GURL("https://news.com/"));

  WebappsUtils::PrunePreScopeNavigationHistory(web_contents(), kScope);

  EXPECT_EQ(GetHistoryUrls(),
            (std::vector<GURL>{GURL("https://example.com/app/a"),
                               GURL("https://news.com/")}));
}

// Same site but outside the app's path: example.com/other/ is NOT in scope.
TEST_F(WebappsUtilsPruneHistoryTest, SameOriginOutsidePathIsPruned) {
  const GURL kScope("https://example.com/app/");
  NavigateAndCommit(GURL("https://example.com/other/"));
  NavigateAndCommit(GURL("https://example.com/app/a"));

  WebappsUtils::PrunePreScopeNavigationHistory(web_contents(), kScope);

  EXPECT_EQ(GetHistoryUrls(),
            (std::vector<GURL>{GURL("https://example.com/app/a")}));
}

// History: app/a, news (left scope), app/b, app/c (current).
// Only the contiguous in-scope suffix [app/b, app/c] should be kept.
TEST_F(WebappsUtilsPruneHistoryTest,
       PrunesEarlierInScopeEntryBeforeOutOfScopeNavigation) {
  const GURL kScope("https://example.com/app/");
  NavigateAndCommit(GURL("https://example.com/app/a"));
  NavigateAndCommit(GURL("https://news.com/"));
  NavigateAndCommit(GURL("https://example.com/app/b"));
  NavigateAndCommit(GURL("https://example.com/app/c"));

  WebappsUtils::PrunePreScopeNavigationHistory(web_contents(), kScope);

  EXPECT_EQ(GetHistoryUrls(),
            (std::vector<GURL>{GURL("https://example.com/app/b"),
                               GURL("https://example.com/app/c")}));
}

// Forward entries after the last committed entry should be preserved when
// pre-scope entries are pruned.
TEST_F(WebappsUtilsPruneHistoryTest, PreservesForwardEntriesAfterCurrent) {
  const GURL kScope("https://example.com/app/");
  NavigateAndCommit(GURL("https://news.com/"));
  NavigateAndCommit(GURL("https://example.com/app/a"));
  NavigateAndCommit(GURL("https://example.com/app/b"));
  content::NavigationSimulator::GoBack(web_contents());

  WebappsUtils::PrunePreScopeNavigationHistory(web_contents(), kScope);

  EXPECT_EQ(GetHistoryUrls(),
            (std::vector<GURL>{GURL("https://example.com/app/a"),
                               GURL("https://example.com/app/b")}));
  EXPECT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 0);
}

}  // namespace webapps
