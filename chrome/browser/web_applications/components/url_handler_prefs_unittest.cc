// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/url_handler_prefs.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

constexpr char kAppUrl1[] = "https://web-app1.com/";
constexpr char kAppUrl2[] = "https://web-app2.com/";
constexpr char kOriginUrl1[] = "https://origin-1.com/abc";
constexpr char kOriginUrl2[] = "https://origin-2.com/abc";
constexpr base::FilePath::CharType kProfile1[] = FILE_PATH_LITERAL("/profile1");
constexpr base::FilePath::CharType kProfile2[] = FILE_PATH_LITERAL("/profile2");

}  // namespace

class UrlHandlerPrefsTest : public ::testing::Test {
 public:
  UrlHandlerPrefsTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {
    app_url_1_ = GURL(kAppUrl1);
    app_url_2_ = GURL(kAppUrl2);
    origin_url_1_ = GURL(kOriginUrl1);
    origin_url_2_ = GURL(kOriginUrl2);
    origin_1_ = url::Origin::Create(origin_url_1_);
    origin_2_ = url::Origin::Create(origin_url_2_);
    profile_1_ = base::FilePath(kProfile1);
    profile_2_ = base::FilePath(kProfile2);
  }

  ~UrlHandlerPrefsTest() override = default;

 protected:
  PrefService* LocalState() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  std::unique_ptr<WebApp> WebAppWithUrlHandlers(
      const GURL& app_url,
      const apps::UrlHandlers& url_handlers) {
    auto web_app = std::make_unique<WebApp>(GenerateAppIdFromURL(app_url));
    web_app->SetName("AppName");
    web_app->SetDisplayMode(DisplayMode::kStandalone);
    web_app->SetStartUrl(app_url);
    web_app->SetUrlHandlers(url_handlers);
    return web_app;
  }

  void CheckMatches(const std::vector<UrlHandlerLaunchParams>& matches,
                    const std::vector<WebApp*>& apps,
                    const std::vector<base::FilePath>& profile_paths) {
    EXPECT_TRUE(matches.size() == apps.size());
    EXPECT_TRUE(matches.size() == profile_paths.size());

    for (size_t i = 0; i < matches.size(); i++) {
      const UrlHandlerLaunchParams& match = matches[i];
      EXPECT_EQ(match.app_id, apps[i]->app_id());
      EXPECT_EQ(match.profile_path, profile_paths[i]);
    }
  }

  GURL app_url_1_;
  GURL app_url_2_;
  GURL origin_url_1_;
  GURL origin_url_2_;
  url::Origin origin_1_;
  url::Origin origin_2_;
  base::FilePath profile_1_;
  base::FilePath profile_2_;

 private:
  ScopedTestingLocalState scoped_testing_local_state_;
};

TEST_F(UrlHandlerPrefsTest, AddAndRemoveApp) {
  const auto web_app =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers());
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    url_handler_prefs::RemoveWebApp(LocalState(), web_app->app_id(),
                                    profile_1_);
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(0u, matches.size());
  }
}

TEST_F(UrlHandlerPrefsTest, AddAndRemoveAppWithPaths) {
  const apps::UrlHandlerInfo handler(origin_1_, false, {"/a*", "/foo"},
                                     {"/b", "/c"});
  const auto web_app = WebAppWithUrlHandlers(app_url_1_, {handler});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers());
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    url_handler_prefs::RemoveWebApp(LocalState(), web_app->app_id(),
                                    profile_1_);
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(0u, matches.size());
  }
}

TEST_F(UrlHandlerPrefsTest, AddAndRemoveAppWithMultipleUrlHandlers) {
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/abc"}, {"/foo"}),
                   apps::UrlHandlerInfo(origin_2_, false, {"/abc"}, {"/foo"})});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers());
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_2_);
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    url_handler_prefs::RemoveWebApp(LocalState(), web_app->app_id(),
                                    profile_1_);
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(0u, matches.size());
  }
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_2_);
    EXPECT_EQ(0u, matches.size());
  }
}

TEST_F(UrlHandlerPrefsTest, MatchContainsInputUrl) {
  const auto web_app =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers());
  auto matches =
      url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
  EXPECT_EQ(1u, matches.size());
  EXPECT_EQ(origin_url_1_, matches[0].url);
}

TEST_F(UrlHandlerPrefsTest, AddMultipleAppsAndRemoveOne) {
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_,
      {apps::UrlHandlerInfo(origin_1_), apps::UrlHandlerInfo(origin_2_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers());
  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_2_,
      {apps::UrlHandlerInfo(origin_1_), apps::UrlHandlerInfo(origin_2_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_1_,
                               web_app_2->url_handlers());
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(2u, matches.size());
    CheckMatches(matches, {web_app_1.get(), web_app_2.get()},
                 {profile_1_, profile_1_});
  }
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_2_);
    EXPECT_EQ(2u, matches.size());
    CheckMatches(matches, {web_app_1.get(), web_app_2.get()},
                 {profile_1_, profile_1_});
  }
  {
    url_handler_prefs::RemoveWebApp(LocalState(), web_app_1->app_id(),
                                    profile_1_);
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app_2.get()}, {profile_1_});
  }
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_2_);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app_2.get()}, {profile_1_});
  }
}

TEST_F(UrlHandlerPrefsTest, RemoveAppNotFound) {
  const auto web_app_1 =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers());
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app_1.get()}, {profile_1_});
  }
  const GURL not_added("https://not-added.com/");
  const auto web_app_2 =
      WebAppWithUrlHandlers(not_added, {apps::UrlHandlerInfo(origin_1_)});
  url_handler_prefs::RemoveWebApp(LocalState(), web_app_2->app_id(),
                                  profile_1_);
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app_1.get()}, {profile_1_});
  }
}

TEST_F(UrlHandlerPrefsTest, OneAppWithManyOrigins) {
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_,
      {apps::UrlHandlerInfo(origin_1_), apps::UrlHandlerInfo(origin_2_)});
  {
    url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                                 web_app->url_handlers());
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_2_);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
}

TEST_F(UrlHandlerPrefsTest, AddAppAgainWithDifferentHandlers) {
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/abc"}, {"/foo"})});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers());
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app_1.get()}, {profile_1_});
  }
  {
    // Excluded, shouldn't match
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("foo"));
    EXPECT_EQ(0u, matches.size());
  }

  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/foo"}, {"/abc"}),
                   apps::UrlHandlerInfo(origin_2_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_1_,
                               web_app_2->url_handlers());
  {
    // Excluded, shouldn't match
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(0u, matches.size());
  }
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("foo"));
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app_2.get()}, {profile_1_});
  }
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_2_);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app_2.get()}, {profile_1_});
  }
}

TEST_F(UrlHandlerPrefsTest, DifferentAppsWithSameHandler) {
  const apps::UrlHandlerInfo handler(origin_1_);
  const auto web_app_1 = WebAppWithUrlHandlers(app_url_1_, {handler});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers());
  const auto web_app_2 = WebAppWithUrlHandlers(app_url_2_, {handler});
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_1_,
                               web_app_2->url_handlers());

  auto matches =
      url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
  EXPECT_EQ(2u, matches.size());
  CheckMatches(matches, {web_app_1.get(), web_app_2.get()},
               {profile_1_, profile_1_});
}

TEST_F(UrlHandlerPrefsTest, MultipleProfiles_Match) {
  const auto web_app_1 =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers());
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_2_,
                               web_app_1->url_handlers());
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(2u, matches.size());
    CheckMatches(matches, {web_app_1.get(), web_app_1.get()},
                 {profile_1_, profile_2_});
  }
  {
    url_handler_prefs::RemoveWebApp(LocalState(), web_app_1->app_id(),
                                    profile_1_);
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app_1.get()}, {profile_2_});
  }
}

TEST_F(UrlHandlerPrefsTest, MultipleProfiles_RemoveProfile) {
  const auto web_app_1 =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers());
  const auto web_app_2 =
      WebAppWithUrlHandlers(app_url_2_, {apps::UrlHandlerInfo(origin_1_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_2_,
                               web_app_1->url_handlers());
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_2_,
                               web_app_2->url_handlers());
  {
    url_handler_prefs::RemoveProfile(LocalState(), profile_2_);
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app_1.get()}, {profile_1_});
  }
  {
    url_handler_prefs::RemoveProfile(LocalState(), profile_1_);
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(0u, matches.size());
  }
}

TEST_F(UrlHandlerPrefsTest, ClearEntries) {
  const auto web_app_1 =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers());
  const auto web_app_2 =
      WebAppWithUrlHandlers(app_url_2_, {apps::UrlHandlerInfo(origin_2_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_2_,
                               web_app_2->url_handlers());
  url_handler_prefs::Clear(LocalState());
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(0u, matches.size());
  }
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_2_);
    EXPECT_EQ(0u, matches.size());
  }
}

TEST_F(UrlHandlerPrefsTest, SubdomainMatch) {
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_,
      {apps::UrlHandlerInfo(origin_1_, /*has_origin_wildcard*/ false)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers());

  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_2_,
      {apps::UrlHandlerInfo(origin_1_, /*has_origin_wildcard*/ true)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_1_,
                               web_app_2->url_handlers());
  {
    // Both handlers should match a URL with an exact origin.
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(2u, matches.size());
    CheckMatches(matches, {web_app_1.get(), web_app_2.get()},
                 {profile_1_, profile_1_});
  }
  {
    // Only the handler that has an origin with wildcard prefix should match a
    // URL that has a longer origin.
    GURL en_origin_url_1("https://en.origin-1.com/abc");
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(LocalState(),
                                                              en_origin_url_1);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app_2.get()}, {profile_1_});
  }
  {
    GURL www_en_origin_url_1("https://www.en.origin-1.com/abc");
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), www_en_origin_url_1);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app_2.get()}, {profile_1_});
  }
}

TEST_F(UrlHandlerPrefsTest, SubdomainMatch_DifferentLevels) {
  GURL en_origin_url_1("https://en.origin-1.com/abc");
  GURL www_en_origin_url_1("https://www.en.origin-1.com/abc");

  // This handler will match "https://*.origin-1.com" urls.
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_,
      {apps::UrlHandlerInfo(origin_1_, /*has_origin_wildcard*/ true)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers());

  url::Origin en_origin_1 = url::Origin::Create(en_origin_url_1);
  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_2_,
      {apps::UrlHandlerInfo(en_origin_1, /*has_origin_wildcard*/ true)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_1_,
                               web_app_2->url_handlers());

  // Both handlers should match a URL that has a longer origin.
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(LocalState(),
                                                              en_origin_url_1);
    EXPECT_EQ(2u, matches.size());
    CheckMatches(matches, {web_app_2.get(), web_app_1.get()},
                 {profile_1_, profile_1_});
  }
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), www_en_origin_url_1);
    EXPECT_EQ(2u, matches.size());
    CheckMatches(matches, {web_app_2.get(), web_app_1.get()},
                 {profile_1_, profile_1_});
  }
}

TEST_F(UrlHandlerPrefsTest, SubdomainMatch_WildcardAsSubdomain) {
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_,
      // Should match https://*.com
      {apps::UrlHandlerInfo(url::Origin::Create(GURL("https://com")),
                            /*has_origin_wildcard*/ true)});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers());

  auto matches = url_handler_prefs::FindMatchingUrlHandlers(
      LocalState(), GURL("https://example.com"));
  EXPECT_EQ(1u, matches.size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});

  matches = url_handler_prefs::FindMatchingUrlHandlers(
      LocalState(), GURL("https://foo.example.com"));
  EXPECT_EQ(1u, matches.size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});

  matches = url_handler_prefs::FindMatchingUrlHandlers(
      LocalState(), GURL("https://example.me"));
  EXPECT_EQ(0u, matches.size());
}

TEST_F(UrlHandlerPrefsTest, MatchPaths) {
  // Test no wildcard
  apps::UrlHandlerInfo handler(origin_1_, false, {"/foo/bar"}, {});
  const auto web_app = WebAppWithUrlHandlers(app_url_1_, {handler});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers());

  // Get origin url without paths
  GURL origin_url = origin_1_.GetURL();
  {
    // Exact match
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("foo/bar"));
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    // "/path/to/" and "/path/to" are different
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("foo/bar/"));
    EXPECT_EQ(0u, matches.size());
  }
  {
    // No match
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("foo"));
    EXPECT_EQ(0u, matches.size());
  }
  {
    // Slash is required at the start of a path to match
    handler = apps::UrlHandlerInfo(origin_1_, false, {"foo/bar"}, {});
    url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                                 {handler});
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("foo/bar"));
    EXPECT_EQ(0u, matches.size());
  }
  {
    // Test wildcard that matches everything
    handler = apps::UrlHandlerInfo(origin_1_, false, {"*"}, {});
    url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                                 {handler});
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("foo/bar/baz"));
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    // Test wildcard with prefix
    handler = apps::UrlHandlerInfo(origin_1_, false, {"/foo/*"}, {});
    url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                                 {handler});
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("foo/bar"));
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("foo/"));
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    // No match because "/foo" and "/foo/" are different
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("foo"));
    EXPECT_EQ(0u, matches.size());
  }
}

TEST_F(UrlHandlerPrefsTest, MatchPathsAndExcludePaths) {
  // No paths and exclude_paths, everything matches.
  apps::UrlHandlerInfo handler(origin_1_);
  const auto web_app = WebAppWithUrlHandlers(app_url_1_, {handler});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers());

  GURL origin_url = origin_1_.GetURL();
  {
    // Get origin url without paths
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("foo/bar"));
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    // Only exclude paths
    handler = apps::UrlHandlerInfo(origin_1_, false, {}, {"/foo/bar"});
    url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                                 {handler});
    // Exact match with the excluded path, not matching
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("foo/bar"));
    EXPECT_EQ(0u, matches.size());
  }
  {
    // Everything else matches
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("foo"));
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    // Both paths and exclude paths exist
    handler = apps::UrlHandlerInfo(origin_1_, false, {"/foo*"}, {"/foo/bar*"});
    url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                                 {handler});
    // Match path and not exclude path
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("foo"));
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    // Match exclude path
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("foo/bar/baz"));
    EXPECT_EQ(0u, matches.size());
  }
  {
    // Doesn't match path or exclude path
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url.Resolve("abc"));
    EXPECT_EQ(0u, matches.size());
  }

  // Not matching if it matches an exclude path, even if it matches a path.
  handler = apps::UrlHandlerInfo(origin_1_, false, {"/foo*"}, {"*"});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               {handler});
  auto matches = url_handler_prefs::FindMatchingUrlHandlers(
      LocalState(), origin_url.Resolve("foo"));
  EXPECT_EQ(0u, matches.size());
}

TEST_F(UrlHandlerPrefsTest, UpdateApp) {
  const auto web_app =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers());
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    url_handler_prefs::UpdateWebApp(LocalState(), web_app->app_id(), profile_1_,
                                    {apps::UrlHandlerInfo(origin_2_)});
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(0u, matches.size());
  }
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_2_);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
}

TEST_F(UrlHandlerPrefsTest, UpdateAppWithPaths) {
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/a"}, {"/b"}),
                   apps::UrlHandlerInfo(origin_2_, false, {"/c"}, {"/d"})});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers());
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_1_.Resolve("a"));
    EXPECT_EQ(1u, matches.size());
  }
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_2_.Resolve("c"));
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    url_handler_prefs::UpdateWebApp(
        LocalState(), web_app->app_id(), profile_1_,
        {apps::UrlHandlerInfo(origin_1_, false, {"/a"}, {"/b"}),
         apps::UrlHandlerInfo(origin_2_, false, {"/foo"}, {"/bar"})});
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_1_.Resolve("a"));
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_2_.Resolve("foo"));
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  {
    // No longer match since it's removed
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_2_.Resolve("c"));
    EXPECT_EQ(0u, matches.size());
  }
}

}  // namespace web_app
