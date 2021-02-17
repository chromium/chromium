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
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
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
 protected:
  void SetUp() override {
    UrlHandlerPrefs::RegisterLocalStatePrefs(test_pref_service_.registry());
    prefs_ = std::make_unique<UrlHandlerPrefs>(&test_pref_service_);

    app_url_1_ = GURL(kAppUrl1);
    app_url_2_ = GURL(kAppUrl2);
    origin_url_1_ = GURL(kOriginUrl1);
    origin_url_2_ = GURL(kOriginUrl2);
    origin_1_ = url::Origin::Create(origin_url_1_);
    origin_2_ = url::Origin::Create(origin_url_2_);
    profile_1_ = base::FilePath(kProfile1);
    profile_2_ = base::FilePath(kProfile2);
  }

  UrlHandlerPrefs& Prefs() { return *prefs_; }

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

  void CheckMatches(
      const base::Optional<std::vector<UrlHandlerPrefs::Match>>& matches,
      const std::vector<WebApp*>& apps,
      const std::vector<base::FilePath>& profile_paths) {
    if (!matches) {
      EXPECT_TRUE(apps.empty());
      EXPECT_TRUE(profile_paths.empty());
    }

    EXPECT_TRUE(matches->size() == apps.size());
    EXPECT_TRUE(matches->size() == profile_paths.size());

    for (size_t i = 0; i < matches->size(); i++) {
      const UrlHandlerPrefs::Match& match = (*matches)[i];
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
  TestingPrefServiceSimple test_pref_service_;
  std::unique_ptr<UrlHandlerPrefs> prefs_;
};

TEST_F(UrlHandlerPrefsTest, AddAndRemoveApp) {
  const auto web_app =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, web_app->url_handlers());
  auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  CheckMatches(matches, {web_app.get()}, {profile_1_});

  Prefs().RemoveWebApp(web_app->app_id(), profile_1_);
  matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());
}

TEST_F(UrlHandlerPrefsTest, AddAndRemoveAppWithPaths) {
  const apps::UrlHandlerInfo handler(origin_1_, false, {"/a*", "/foo"},
                                     {"/b", "/c"});
  const auto web_app = WebAppWithUrlHandlers(app_url_1_, {handler});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, web_app->url_handlers());
  auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  CheckMatches(matches, {web_app.get()}, {profile_1_});

  Prefs().RemoveWebApp(web_app->app_id(), profile_1_);
  matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());
}

TEST_F(UrlHandlerPrefsTest, AddAndRemoveAppWithMultipleUrlHandlers) {
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/abc"}, {"/foo"}),
                   apps::UrlHandlerInfo(origin_2_, false, {"/abc"}, {"/foo"})});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, web_app->url_handlers());
  auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  CheckMatches(matches, {web_app.get()}, {profile_1_});
  matches = Prefs().FindMatchingUrlHandlers(origin_url_2_);
  CheckMatches(matches, {web_app.get()}, {profile_1_});

  Prefs().RemoveWebApp(web_app->app_id(), profile_1_);
  matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());
  matches = Prefs().FindMatchingUrlHandlers(origin_url_2_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());
}

TEST_F(UrlHandlerPrefsTest, AddMultipleAppsAndRemoveOne) {
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_,
      {apps::UrlHandlerInfo(origin_1_), apps::UrlHandlerInfo(origin_2_)});
  Prefs().AddWebApp(web_app_1->app_id(), profile_1_, web_app_1->url_handlers());
  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_2_,
      {apps::UrlHandlerInfo(origin_1_), apps::UrlHandlerInfo(origin_2_)});
  Prefs().AddWebApp(web_app_2->app_id(), profile_1_, web_app_2->url_handlers());
  auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(2u, matches->size());
  CheckMatches(matches, {web_app_1.get(), web_app_2.get()},
               {profile_1_, profile_1_});
  matches = Prefs().FindMatchingUrlHandlers(origin_url_2_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(2u, matches->size());
  CheckMatches(matches, {web_app_1.get(), web_app_2.get()},
               {profile_1_, profile_1_});

  Prefs().RemoveWebApp(web_app_1->app_id(), profile_1_);
  matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app_2.get()}, {profile_1_});
  matches = Prefs().FindMatchingUrlHandlers(origin_url_2_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app_2.get()}, {profile_1_});
}

TEST_F(UrlHandlerPrefsTest, RemoveAppNotFound) {
  const auto web_app_1 =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  Prefs().AddWebApp(web_app_1->app_id(), profile_1_, web_app_1->url_handlers());
  auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app_1.get()}, {profile_1_});

  const GURL not_added("https://not-added.com/");
  const auto web_app_2 =
      WebAppWithUrlHandlers(not_added, {apps::UrlHandlerInfo(origin_1_)});
  Prefs().RemoveWebApp(web_app_2->app_id(), profile_1_);
  matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app_1.get()}, {profile_1_});
}

TEST_F(UrlHandlerPrefsTest, OneAppWithManyOrigins) {
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_,
      {apps::UrlHandlerInfo(origin_1_), apps::UrlHandlerInfo(origin_2_)});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, web_app->url_handlers());

  auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});

  matches = Prefs().FindMatchingUrlHandlers(origin_url_2_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});
}

TEST_F(UrlHandlerPrefsTest, AddAppAgainWithDifferentHandlers) {
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/abc"}, {"/foo"})});
  Prefs().AddWebApp(web_app_1->app_id(), profile_1_, web_app_1->url_handlers());
  auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app_1.get()}, {profile_1_});

  // Excluded, shouldn't match
  matches = Prefs().FindMatchingUrlHandlers(origin_1_.GetURL().Resolve("foo"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());

  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/foo"}, {"/abc"}),
                   apps::UrlHandlerInfo(origin_2_)});
  Prefs().AddWebApp(web_app_2->app_id(), profile_1_, web_app_2->url_handlers());

  // Excluded, shouldn't match
  matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());

  matches = Prefs().FindMatchingUrlHandlers(origin_1_.GetURL().Resolve("foo"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app_2.get()}, {profile_1_});

  matches = Prefs().FindMatchingUrlHandlers(origin_url_2_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app_2.get()}, {profile_1_});
}

TEST_F(UrlHandlerPrefsTest, DifferentAppsWithSameHandler) {
  const apps::UrlHandlerInfo handler(origin_1_);
  const auto web_app_1 = WebAppWithUrlHandlers(app_url_1_, {handler});
  Prefs().AddWebApp(web_app_1->app_id(), profile_1_, web_app_1->url_handlers());
  const auto web_app_2 = WebAppWithUrlHandlers(app_url_2_, {handler});
  Prefs().AddWebApp(web_app_2->app_id(), profile_1_, web_app_2->url_handlers());

  const auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(2u, matches->size());
  CheckMatches(matches, {web_app_1.get(), web_app_2.get()},
               {profile_1_, profile_1_});
}

TEST_F(UrlHandlerPrefsTest, MultipleProfiles_Match) {
  const auto web_app_1 =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  Prefs().AddWebApp(web_app_1->app_id(), profile_1_, web_app_1->url_handlers());
  Prefs().AddWebApp(web_app_1->app_id(), profile_2_, web_app_1->url_handlers());

  auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(2u, matches->size());
  CheckMatches(matches, {web_app_1.get(), web_app_1.get()},
               {profile_1_, profile_2_});

  Prefs().RemoveWebApp(web_app_1->app_id(), profile_1_);
  matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app_1.get()}, {profile_2_});
}

TEST_F(UrlHandlerPrefsTest, MultipleProfiles_RemoveProfile) {
  const auto web_app_1 =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  Prefs().AddWebApp(web_app_1->app_id(), profile_1_, web_app_1->url_handlers());
  const auto web_app_2 =
      WebAppWithUrlHandlers(app_url_2_, {apps::UrlHandlerInfo(origin_1_)});
  Prefs().AddWebApp(web_app_1->app_id(), profile_2_, web_app_1->url_handlers());
  Prefs().AddWebApp(web_app_2->app_id(), profile_2_, web_app_2->url_handlers());

  Prefs().RemoveProfile(profile_2_);
  auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app_1.get()}, {profile_1_});

  Prefs().RemoveProfile(profile_1_);
  matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());
}

TEST_F(UrlHandlerPrefsTest, ClearEntries) {
  const auto web_app_1 =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  Prefs().AddWebApp(web_app_1->app_id(), profile_1_, web_app_1->url_handlers());
  const auto web_app_2 =
      WebAppWithUrlHandlers(app_url_2_, {apps::UrlHandlerInfo(origin_2_)});
  Prefs().AddWebApp(web_app_2->app_id(), profile_2_, web_app_2->url_handlers());
  Prefs().Clear();
  auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());
  matches = Prefs().FindMatchingUrlHandlers(origin_url_2_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());
}

TEST_F(UrlHandlerPrefsTest, SubdomainMatch) {
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_,
      {apps::UrlHandlerInfo(origin_1_, /*has_origin_wildcard*/ false)});
  Prefs().AddWebApp(web_app_1->app_id(), profile_1_, web_app_1->url_handlers());

  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_2_,
      {apps::UrlHandlerInfo(origin_1_, /*has_origin_wildcard*/ true)});
  Prefs().AddWebApp(web_app_2->app_id(), profile_1_, web_app_2->url_handlers());

  // Both handlers should match a URL with an exact origin.
  auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(2u, matches->size());
  CheckMatches(matches, {web_app_1.get(), web_app_2.get()},
               {profile_1_, profile_1_});

  // Only the handler that has an origin with wildcard prefix should match a URL
  // that has a longer origin.
  GURL en_origin_url_1("https://en.origin-1.com/abc");
  GURL www_en_origin_url_1("https://www.en.origin-1.com/abc");
  matches = Prefs().FindMatchingUrlHandlers(en_origin_url_1);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app_2.get()}, {profile_1_});

  matches = Prefs().FindMatchingUrlHandlers(www_en_origin_url_1);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app_2.get()}, {profile_1_});
}

TEST_F(UrlHandlerPrefsTest, SubdomainMatch_DifferentLevels) {
  GURL en_origin_url_1("https://en.origin-1.com/abc");
  GURL www_en_origin_url_1("https://www.en.origin-1.com/abc");

  // This handler will match "https://*.origin-1.com" urls.
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_,
      {apps::UrlHandlerInfo(origin_1_, /*has_origin_wildcard*/ true)});
  Prefs().AddWebApp(web_app_1->app_id(), profile_1_, web_app_1->url_handlers());

  url::Origin en_origin_1 = url::Origin::Create(en_origin_url_1);
  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_2_,
      {apps::UrlHandlerInfo(en_origin_1, /*has_origin_wildcard*/ true)});
  Prefs().AddWebApp(web_app_2->app_id(), profile_1_, web_app_2->url_handlers());

  // Both handlers should match a URL that has a longer origin.
  auto matches = Prefs().FindMatchingUrlHandlers(en_origin_url_1);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(2u, matches->size());
  CheckMatches(matches, {web_app_2.get(), web_app_1.get()},
               {profile_1_, profile_1_});

  matches = Prefs().FindMatchingUrlHandlers(www_en_origin_url_1);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(2u, matches->size());
  CheckMatches(matches, {web_app_2.get(), web_app_1.get()},
               {profile_1_, profile_1_});
}

TEST_F(UrlHandlerPrefsTest, MatchPaths) {
  // Test no wildcard
  apps::UrlHandlerInfo handler(origin_1_, false, {"/foo/bar"}, {});
  const auto web_app = WebAppWithUrlHandlers(app_url_1_, {handler});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, web_app->url_handlers());

  // Get origin url without paths
  GURL origin_url = origin_1_.GetURL();
  // Exact match
  auto matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo/bar"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});

  // "/path/to/" and "/path/to" are different
  matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo/bar/"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());

  // No match
  matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());

  // Slash is required at the start of a path to match
  handler = apps::UrlHandlerInfo(origin_1_, false, {"foo/bar"}, {});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, {handler});
  matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo/bar"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());

  // Test wildcard that matches everything
  handler = apps::UrlHandlerInfo(origin_1_, false, {"*"}, {});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, {handler});
  matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});
  matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo/bar/baz"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});

  // Test wildcard with prefix
  handler = apps::UrlHandlerInfo(origin_1_, false, {"/foo/*"}, {});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, {handler});
  matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo/bar"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});
  matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo/"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});
  // No match because "/foo" and "/foo/" are different
  matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());
}

TEST_F(UrlHandlerPrefsTest, MatchPathsAndExcludePaths) {
  // No paths and exclude_paths, everything matches.
  apps::UrlHandlerInfo handler(origin_1_);
  const auto web_app = WebAppWithUrlHandlers(app_url_1_, {handler});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, web_app->url_handlers());
  // Get origin url without paths
  GURL origin_url = origin_1_.GetURL();
  auto matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo/bar"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});

  // Only exclude paths
  handler = apps::UrlHandlerInfo(origin_1_, false, {}, {"/foo/bar"});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, {handler});
  // Exact match with the excluded path, not matching
  matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo/bar"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());
  // Everything else matches
  matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});

  // Both paths and exclude paths exist
  handler = apps::UrlHandlerInfo(origin_1_, false, {"/foo*"}, {"/foo/bar*"});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, {handler});
  // Match path and not exclude path
  matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});
  // Match exclude path
  matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo/bar/baz"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());
  // Doesn't match path or exclude path
  matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("abc"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());

  // Not matching if it matches an exclude path, even if it matches a path.
  handler = apps::UrlHandlerInfo(origin_1_, false, {"/foo*"}, {"*"});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, {handler});
  matches = Prefs().FindMatchingUrlHandlers(origin_url.Resolve("foo"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());
}

TEST_F(UrlHandlerPrefsTest, UpdateApp) {
  const auto web_app =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, web_app->url_handlers());
  auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});

  Prefs().UpdateWebApp(web_app->app_id(), profile_1_,
                       {apps::UrlHandlerInfo(origin_2_)});
  matches = Prefs().FindMatchingUrlHandlers(origin_url_1_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());
  matches = Prefs().FindMatchingUrlHandlers(origin_url_2_);
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});
}

TEST_F(UrlHandlerPrefsTest, UpdateAppWithPaths) {
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/a"}, {"/b"}),
                   apps::UrlHandlerInfo(origin_2_, false, {"/c"}, {"/d"})});
  Prefs().AddWebApp(web_app->app_id(), profile_1_, web_app->url_handlers());
  auto matches = Prefs().FindMatchingUrlHandlers(origin_url_1_.Resolve("a"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  matches = Prefs().FindMatchingUrlHandlers(origin_url_2_.Resolve("c"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});

  Prefs().UpdateWebApp(
      web_app->app_id(), profile_1_,
      {apps::UrlHandlerInfo(origin_1_, false, {"/a"}, {"/b"}),
       apps::UrlHandlerInfo(origin_2_, false, {"/foo"}, {"/bar"})});
  matches = Prefs().FindMatchingUrlHandlers(origin_url_1_.Resolve("a"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});
  matches = Prefs().FindMatchingUrlHandlers(origin_url_2_.Resolve("foo"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(1u, matches->size());
  CheckMatches(matches, {web_app.get()}, {profile_1_});
  // No longer match since it's removed
  matches = Prefs().FindMatchingUrlHandlers(origin_url_2_.Resolve("c"));
  EXPECT_TRUE(matches.has_value());
  EXPECT_EQ(0u, matches->size());
}

}  // namespace web_app
