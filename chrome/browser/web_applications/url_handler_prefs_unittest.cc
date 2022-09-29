// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/url_handler_prefs.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/common/pref_names.h"
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
constexpr char kOriginUrl3[] = "https://origin-3.com/abc";
constexpr char kTime1[] = "1 Jan 2000 00:00:00 GMT";
constexpr char kTime2[] = "2 Jan 2000 00:00:00 GMT";
constexpr char kTime3[] = "3 Jan 2000 00:00:00 GMT";
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
    origin_url_3_ = GURL(kOriginUrl3);
    origin_1_ = url::Origin::Create(origin_url_1_);
    origin_2_ = url::Origin::Create(origin_url_2_);
    origin_3_ = url::Origin::Create(origin_url_3_);
    profile_1_ = base::FilePath(kProfile1);
    profile_2_ = base::FilePath(kProfile2);
    EXPECT_TRUE(base::Time::FromString(kTime1, &time_1_));
    EXPECT_TRUE(base::Time::FromString(kTime2, &time_2_));
    EXPECT_TRUE(base::Time::FromString(kTime3, &time_3_));
  }

  ~UrlHandlerPrefsTest() override = default;

 protected:
  PrefService* LocalState() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  void ExpectUrlHandlerPrefs(const std::string& expected_prefs) {
    const base::Value& stored_prefs =
        LocalState()->GetValue(prefs::kWebAppsUrlHandlerInfo);
    const base::Value expected_prefs_value =
        base::test::ParseJson(expected_prefs);
    EXPECT_EQ(stored_prefs, expected_prefs_value);
  }

  std::unique_ptr<WebApp> WebAppWithUrlHandlers(
      const GURL& app_url,
      const apps::UrlHandlers& url_handlers) {
    auto web_app = std::make_unique<WebApp>(
        GenerateAppId(/*manifest_id=*/absl::nullopt, app_url));
    web_app->SetName("AppName");
    web_app->SetDisplayMode(DisplayMode::kStandalone);
    web_app->SetStartUrl(app_url);
    web_app->SetUrlHandlers(url_handlers);
    return web_app;
  }

  void CheckMatches(const std::vector<UrlHandlerLaunchParams>& matches,
                    const std::vector<WebApp*>& apps,
                    const std::vector<base::FilePath>& profile_paths) {
    EXPECT_EQ(matches.size(), apps.size());
    EXPECT_EQ(matches.size(), profile_paths.size());

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
  GURL origin_url_3_;
  url::Origin origin_1_;
  url::Origin origin_2_;
  url::Origin origin_3_;
  base::FilePath profile_1_;
  base::FilePath profile_2_;
  base::Time time_1_;
  base::Time time_2_;
  base::Time time_3_;

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

// Tests that choices can be saved when paths are not specified in
// web-app-origin-association and the default path "/*" is assumed.
TEST_F(UrlHandlerPrefsTest, SaveUserChoice_DefaultPaths) {
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers(), time_1_);
  {
    // Check default choice and timestamp.
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kNone);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_1_);
  }
  // Save choice as UrlHandlerSavedChoice::kInApp.
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app->app_id(), profile_1_,
                                   origin_url_1_, time_1_);
  {
    // Check saved choice and timestamp.
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kInApp);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_1_);
  }
}

// Check for the saved choice and timestamp.
TEST_F(UrlHandlerPrefsTest, SaveUserChoice_InApp) {
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_,
      {apps::UrlHandlerInfo(origin_1_, false, {"/abc", "/def"}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers(), time_1_);
  // Save choice as UrlHandlerSavedChoice::kInApp to "/abc" path.
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app->app_id(), profile_1_,
                                   origin_1_.GetURL().Resolve("abc"), time_2_);
  {
    // Check saved choice and timestamp.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("abc"));
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kInApp);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_2_);
  }
  {
    // Check unaffected path.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("def"));
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kNone);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_1_);
  }
}

// Saving as UrlHandlerSavedChoice::kInBrowser prevents an app from being
// matched as a URL handler.
TEST_F(UrlHandlerPrefsTest, SaveUserChoice_InBrowser) {
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/abc"}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers());
  {
    // Expect installed app to be matched with no saved choice.
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kNone);
  }
  url_handler_prefs::SaveOpenInBrowser(LocalState(), origin_url_1_, time_1_);
  {
    // Expect the same URL to not be matched.
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(0u, matches.size());
  }
}

TEST_F(UrlHandlerPrefsTest, SaveUserChoice_HasExcludePaths) {
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_,
      {apps::UrlHandlerInfo(origin_1_, false, {"/a", "/b"}, {"/x"})});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers(), time_1_);
  // Save choice as UrlHandlerSavedChoice::kInApp for "/a" path.
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app->app_id(), profile_1_,
                                   origin_1_.GetURL().Resolve("a"), time_2_);
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("a"));
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kInApp);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_2_);
  }
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("b"));
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kNone);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_1_);
  }
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("x"));
    EXPECT_EQ(0u, matches.size());
  }
}

// Saved choices can be get overwritten.
TEST_F(UrlHandlerPrefsTest, SaveUserChoice_Overwrite) {
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/*", "/a"}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers());

  // URL with "/b" path matches and saves to "/*" URL handler path.
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app->app_id(), profile_1_,
                                   origin_1_.GetURL().Resolve("b"), time_1_);
  // URL with "/a" path matches and saves to both URL handler paths.
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app->app_id(), profile_1_,
                                   origin_1_.GetURL().Resolve("a"), time_2_);
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("b"));
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kInApp);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_2_);
  }
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("a"));
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kInApp);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_2_);
  }
}

TEST_F(UrlHandlerPrefsTest, SaveUserChoice_MultipleApps) {
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/*"}, {})});
  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_2_, {apps::UrlHandlerInfo(origin_1_, false, {"/*"}, {})});

  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers(), time_1_);
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_1_,
                               web_app_2->url_handlers(), time_1_);
  {
    // Both apps should match the input URL.
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    ASSERT_EQ(2u, matches.size());
    EXPECT_EQ(matches[0].app_id, web_app_1->app_id());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kNone);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_1_);
    EXPECT_EQ(matches[1].app_id, web_app_2->app_id());
    EXPECT_EQ(matches[1].saved_choice, UrlHandlerSavedChoice::kNone);
    EXPECT_EQ(matches[1].saved_choice_timestamp, time_1_);
  }
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app_1->app_id(),
                                   profile_1_, origin_url_1_, time_2_);
  {
    // Only the app with a path saved as UrlHandlerSavedChoice::kInApp is
    // returned from matching.
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].app_id, web_app_1->app_id());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kInApp);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_2_);
  }
}

TEST_F(UrlHandlerPrefsTest, SaveUserChoice_MultipleAppsInBrowser) {
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/*"}, {})});
  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_2_, {apps::UrlHandlerInfo(origin_1_, false, {"/*"}, {})});

  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers(), time_1_);
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_1_,
                               web_app_2->url_handlers(), time_1_);

  url_handler_prefs::SaveOpenInBrowser(LocalState(), origin_url_1_, time_2_);
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    EXPECT_EQ(0u, matches.size());
  }
}

TEST_F(UrlHandlerPrefsTest, SaveUserChoice_OriginWildcardMatch) {
  const auto web_app = WebAppWithUrlHandlers(
      app_url_2_,
      {apps::UrlHandlerInfo(origin_1_, /*has_origin_wildcard*/ true)});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers());

  GURL en_origin_url_1("https://en.origin-1.com/abc");
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(LocalState(),
                                                              en_origin_url_1);
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
  }
  // Choice should also be saved successfully to the app that matches because of
  // its origin wildcard.
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app->app_id(), profile_1_,
                                   en_origin_url_1, time_1_);
  {
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kInApp);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_1_);
  }

  url_handler_prefs::SaveOpenInBrowser(LocalState(), en_origin_url_1, time_1_);
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(LocalState(),
                                                              en_origin_url_1);
    EXPECT_EQ(0u, matches.size());
  }
}

TEST_F(UrlHandlerPrefsTest, SaveUserChoiceInAppAndInstallNewApp) {
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/abc"}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers(), time_1_);
  // Save choice as UrlHandlerSavedChoice::kInApp to "/abc" path.
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app_1->app_id(),
                                   profile_1_,
                                   origin_1_.GetURL().Resolve("abc"), time_1_);
  {
    // Check saved choice.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("abc"));
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ(matches.front().saved_choice, UrlHandlerSavedChoice::kInApp);
    EXPECT_EQ(matches.front().app_id, web_app_1->app_id());
    EXPECT_EQ(matches.front().saved_choice_timestamp, time_1_);
  }

  // Install another app.
  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_2_, {apps::UrlHandlerInfo(origin_1_, false, {"/abc"}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_1_,
                               web_app_2->url_handlers(), time_2_);
  {
    // Check now there should be two matches.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("abc"));
    ASSERT_EQ(2u, matches.size());
  }

  // Save the new app as the default choice.
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app_2->app_id(),
                                   profile_1_,
                                   origin_1_.GetURL().Resolve("abc"), time_2_);
  {
    // Verify the new saved choice.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("abc"));
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ(matches.front().saved_choice, UrlHandlerSavedChoice::kInApp);
    EXPECT_EQ(matches.front().app_id, web_app_2->app_id());
    EXPECT_EQ(matches.front().saved_choice_timestamp, time_2_);
  }
}

TEST_F(UrlHandlerPrefsTest, SaveUserChoiceInBrowserAndInstallNewApp) {
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/abc"}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers(), time_1_);
  // Save choice to open in browser.
  url_handler_prefs::SaveOpenInBrowser(
      LocalState(), origin_1_.GetURL().Resolve("abc"), time_1_);
  {
    // Check saved choice.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("abc"));
    ASSERT_EQ(0u, matches.size());
  }

  // Install another app.
  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_2_, {apps::UrlHandlerInfo(origin_1_, false, {"/abc"}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_1_,
                               web_app_2->url_handlers(), time_2_);
  {
    // Check there are now two matches.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_1_.GetURL().Resolve("abc"));
    ASSERT_EQ(2u, matches.size());
  }
}

TEST_F(UrlHandlerPrefsTest, SaveUserChoice_InBrowserThenInApp) {
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/*"}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers(), time_1_);

  url_handler_prefs::SaveOpenInBrowser(LocalState(), origin_url_1_, time_1_);
  {
    ExpectUrlHandlerPrefs(R"({
      "https://origin-1.com": [ {
        "app_id":"hfbpnmjjjooicehokhgjihcnkmbbpefl",
        "exclude_paths": [  ],
        "has_origin_wildcard": false,
        "include_paths": [ {
          "choice": 0,
          "path": "/*",
          "timestamp": "12591158400000000"
        } ],
        "profile_path": "/profile1"
      } ]
    })");
  }

  // Install a second app that also handles origin_1_/*.
  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_2_, {apps::UrlHandlerInfo(origin_1_, false, {"/*"}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_1_,
                               web_app_2->url_handlers(), time_2_);
  // Now save to open origin_1_/* in web_app_2.
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app_2->app_id(),
                                   profile_1_, origin_url_1_, time_2_);
  {
    // web_app_1 and web_app_2 both can handle origin_1_/*. Since we now saved
    // web_app_2 as the default handler app, the "/*" path of web_app_2 is
    // saved as kInApp, and "/*" of web_app_1 is reset to kNone. Timestamps
    // are updated to time_2_.
    ExpectUrlHandlerPrefs(R"({
      "https://origin-1.com": [ {
        "app_id":"hfbpnmjjjooicehokhgjihcnkmbbpefl",
        "exclude_paths": [  ],
        "has_origin_wildcard": false,
        "include_paths": [ {
          "choice": 1,
          "path": "/*",
          "timestamp": "12591244800000000"
        } ],
        "profile_path": "/profile1"
      }, {
        "app_id":"dioomdeompgjpnegoidgaopfdnbbljlb",
        "exclude_paths": [  ],
        "has_origin_wildcard": false,
        "include_paths": [ {
          "choice": 2,
          "path": "/*",
          "timestamp": "12591244800000000"
        } ],
        "profile_path": "/profile1"
      } ]
    })");
    auto matches =
        url_handler_prefs::FindMatchingUrlHandlers(LocalState(), origin_url_1_);
    CheckMatches(matches, {web_app_2.get()}, {profile_1_});
  }
}

// Updating an app with a new handler should preserve a previously added handler
// and previously saved user choice.
TEST_F(UrlHandlerPrefsTest, UpdateAppWithSavedChoice_AddHandler) {
  // Set up 2 existing handler entries with saved choices.
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/a"}, {"/b"}),
                   apps::UrlHandlerInfo(origin_2_, false, {"/c"}, {"/d"})});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers(), time_1_);
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app->app_id(), profile_1_,
                                   origin_1_.GetURL().Resolve("a"), time_2_);
  url_handler_prefs::SaveOpenInBrowser(
      LocalState(), origin_2_.GetURL().Resolve("c"), time_2_);

  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/b" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 2,
        "path": "/a",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ],
    "https://origin-2.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/d" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 0,
        "path": "/c",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");

  // Update app to include an additional handler entry.
  url_handler_prefs::UpdateWebApp(
      LocalState(), web_app->app_id(), profile_1_,
      {apps::UrlHandlerInfo(origin_1_, false, {"/a"}, {"/b"}),
       apps::UrlHandlerInfo(origin_2_, false, {"/c"}, {"/d"}),
       apps::UrlHandlerInfo(origin_3_, false, {"/e"}, {"/f"})},
      time_3_);
  {
    // Origin 1 handler entry unchanged.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_1_.Resolve("a"));
    EXPECT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].app_id, web_app->app_id());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kInApp);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_2_);
  }
  {
    // Origin 2 handler entry unchanged.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_2_.Resolve("c"));
    EXPECT_EQ(0u, matches.size());
  }
  {
    // New origin 3 handler entry has no saved choice.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_3_.Resolve("e"));
    EXPECT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].app_id, web_app->app_id());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kNone);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_3_);
  }
  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/b" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 2,
        "path": "/a",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ],
    "https://origin-2.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/d" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 0,
        "path": "/c",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ],
    "https://origin-3.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/f" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 1,
        "path": "/e",
        "timestamp": "12591331200000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");
}

TEST_F(UrlHandlerPrefsTest, UpdateAppWithSavedChoice_RemoveHandler) {
  // Set up 3 existing handler entries.
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/a"}, {"/b"}),
                   apps::UrlHandlerInfo(origin_2_, false, {"/c"}, {"/d"}),
                   apps::UrlHandlerInfo(origin_3_, false, {"/e"}, {"/f"})});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers(), time_1_);
  // Set saved choices.
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app->app_id(), profile_1_,
                                   origin_1_.GetURL().Resolve("a"), time_2_);
  url_handler_prefs::SaveOpenInBrowser(
      LocalState(), origin_2_.GetURL().Resolve("c"), time_2_);

  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/b" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 2,
        "path": "/a",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ],
    "https://origin-2.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/d" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 0,
        "path": "/c",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ],
    "https://origin-3.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/f" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 1,
        "path": "/e",
        "timestamp": "12591158400000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");

  // Update app to remove one handler entry.
  url_handler_prefs::UpdateWebApp(
      LocalState(), web_app->app_id(), profile_1_,
      {apps::UrlHandlerInfo(origin_1_, false, {"/a"}, {"/b"}),
       apps::UrlHandlerInfo(origin_2_, false, {"/c"}, {"/d"})},
      time_3_);
  {
    // Origin 1 handler entry unchanged.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_1_.Resolve("a"));
    EXPECT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].app_id, web_app->app_id());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kInApp);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_2_);
  }
  {
    // Origin 2 handler entry unchanged.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_2_.Resolve("c"));
    EXPECT_EQ(0u, matches.size());
  }
  {
    // Origin 3 handler entry removed.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_3_.Resolve("e"));
    EXPECT_EQ(0u, matches.size());
  }
  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/b" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 2,
        "path": "/a",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ],
    "https://origin-2.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/d" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 0,
        "path": "/c",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");
}

TEST_F(UrlHandlerPrefsTest, UpdateAppWithSavedChoice_ChangeIncludePaths) {
  // Set up handler entry with 1 existing include_path.
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/a"}, {"/b"})});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers(), time_1_);
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app->app_id(), profile_1_,
                                   origin_url_1_.Resolve("a"), time_2_);

  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/b" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 2,
        "path": "/a",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");

  // Update existing handler entry with additional include_path.
  url_handler_prefs::UpdateWebApp(
      LocalState(), web_app->app_id(), profile_1_,
      {apps::UrlHandlerInfo(origin_1_, false, {"/a", "/c"}, {"/b"})}, time_3_);
  {
    // Previous include_path has reset choice and new timestamp.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_1_.Resolve("a"));
    EXPECT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].app_id, web_app->app_id());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kNone);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_3_);
  }
  {
    // New include_path has default choice and new timestamp.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_1_.Resolve("c"));
    EXPECT_EQ(1u, matches.size());
    CheckMatches(matches, {web_app.get()}, {profile_1_});
    EXPECT_EQ(matches[0].app_id, web_app->app_id());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kNone);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_3_);
  }
  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/b" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 1,
        "path": "/a",
        "timestamp": "12591331200000000"
      }, {
        "choice": 1,
        "path": "/c",
        "timestamp": "12591331200000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");
}

TEST_F(UrlHandlerPrefsTest, UpdateAppWithSavedChoice_ChangeExcludePaths) {
  // Set up handler entry with 1 existing include_path and 1 exclude_path.
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/a"}, {"/b"})});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers(), time_1_);
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app->app_id(), profile_1_,
                                   origin_url_1_.Resolve("a"), time_2_);

  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/b" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 2,
        "path": "/a",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");

  // Update existing handler entry with additional exclude_path.
  url_handler_prefs::UpdateWebApp(
      LocalState(), web_app->app_id(), profile_1_,
      {apps::UrlHandlerInfo(origin_1_, false, {"/a"}, {"/b", "/c"})}, time_3_);
  {
    // Existing include_path unchanged.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_1_.Resolve("a"));
    EXPECT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].app_id, web_app->app_id());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kInApp);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_2_);
  }
  {
    // Existing exclude_path.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_1_.Resolve("b"));
    EXPECT_EQ(0u, matches.size());
  }
  {
    // New exclude_path.
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_1_.Resolve("c"));
    EXPECT_EQ(0u, matches.size());
  }
  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/b", "/c" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 2,
        "path": "/a",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");
}

TEST_F(UrlHandlerPrefsTest, UpdateAppWithSavedChoice_SubdomainMatch) {
  GURL en_origin_url_1("https://en.origin-1.com/a");
  // Set up handler entry with 1 existing include_path.
  const auto web_app = WebAppWithUrlHandlers(
      app_url_1_,
      {apps::UrlHandlerInfo(origin_1_, /*has_origin_wildcard=*/false, {"/a"},
                            {"/b"})});
  url_handler_prefs::AddWebApp(LocalState(), web_app->app_id(), profile_1_,
                               web_app->url_handlers(), time_1_);
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app->app_id(), profile_1_,
                                   en_origin_url_1, time_2_);
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(
        LocalState(), origin_url_1_.Resolve("a"));
    EXPECT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].app_id, web_app->app_id());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kInApp);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_2_);
  }

  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/b" ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 2,
        "path": "/a",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");

  // Update existing handler entry to an origin with prefix wildcard.
  url_handler_prefs::UpdateWebApp(
      LocalState(), web_app->app_id(), profile_1_,
      {apps::UrlHandlerInfo(origin_1_, /*has_origin_wildcard=*/true, {"/a"},
                            {"/b"})},
      time_3_);
  {
    auto matches = url_handler_prefs::FindMatchingUrlHandlers(LocalState(),
                                                              en_origin_url_1);
    EXPECT_EQ(1u, matches.size());
    EXPECT_EQ(matches[0].app_id, web_app->app_id());
    EXPECT_EQ(matches[0].saved_choice, UrlHandlerSavedChoice::kNone);
    EXPECT_EQ(matches[0].saved_choice_timestamp, time_3_);
  }
  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ "/b" ],
      "has_origin_wildcard": true,
      "include_paths": [ {
        "choice": 1,
        "path": "/a",
        "timestamp": "12591331200000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");
}

TEST_F(UrlHandlerPrefsTest, ProfileHasUrlHandlers) {
  // No profiles have apps installed.
  EXPECT_FALSE(
      url_handler_prefs::ProfileHasUrlHandlers(LocalState(), profile_1_));
  EXPECT_FALSE(
      url_handler_prefs::ProfileHasUrlHandlers(LocalState(), profile_2_));

  // Profile 1 has web_app_1 installed.
  const auto web_app_1 =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers());
  EXPECT_TRUE(
      url_handler_prefs::ProfileHasUrlHandlers(LocalState(), profile_1_));
  EXPECT_FALSE(
      url_handler_prefs::ProfileHasUrlHandlers(LocalState(), profile_2_));

  // Profile 1 has web_app_1 installed -and- Profile 2 has web_app_2 installed.
  const auto web_app_2 =
      WebAppWithUrlHandlers(app_url_2_, {apps::UrlHandlerInfo(origin_2_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_2_,
                               web_app_2->url_handlers());
  EXPECT_TRUE(
      url_handler_prefs::ProfileHasUrlHandlers(LocalState(), profile_1_));
  EXPECT_TRUE(
      url_handler_prefs::ProfileHasUrlHandlers(LocalState(), profile_2_));

  // No profiles have apps installed.
  url_handler_prefs::RemoveWebApp(LocalState(), web_app_1->app_id(),
                                  profile_1_);
  url_handler_prefs::RemoveWebApp(LocalState(), web_app_2->app_id(),
                                  profile_2_);
  EXPECT_FALSE(
      url_handler_prefs::ProfileHasUrlHandlers(LocalState(), profile_1_));
  EXPECT_FALSE(
      url_handler_prefs::ProfileHasUrlHandlers(LocalState(), profile_2_));
}

TEST_F(UrlHandlerPrefsTest, ResetSavedChoice_InApp) {
  // Profile 1 has web_app_1 installed. Profile 2 has web_app_2 installed.
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/a", "/b"}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers(), time_1_);
  const auto web_app_2 =
      WebAppWithUrlHandlers(app_url_2_, {apps::UrlHandlerInfo(origin_2_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_2_,
                               web_app_2->url_handlers(), time_1_);

  // Save all paths as UrlHandlerSavedChoice::kInApp.
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app_1->app_id(),
                                   profile_1_, origin_url_1_.Resolve("/a"),
                                   time_2_);
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app_1->app_id(),
                                   profile_1_, origin_url_1_.Resolve("/b"),
                                   time_2_);
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app_2->app_id(),
                                   profile_2_, origin_url_2_, time_2_);

  constexpr char prefs_before_invalid_resets[] = R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 2,
        "path": "/a",
        "timestamp": "12591244800000000"
      },
      {
        "choice": 2,
        "path": "/b",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ],
    "https://origin-2.com": [ {
      "app_id": "dioomdeompgjpnegoidgaopfdnbbljlb",
      "exclude_paths": [ ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 2,
        "path": "/*",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile2"
    } ]
  })";
  ExpectUrlHandlerPrefs(prefs_before_invalid_resets);

  // Try reset with invalid input.
  // /x path doesn't exist.
  url_handler_prefs::ResetSavedChoice(
      LocalState(), web_app_1->app_id(), profile_1_, "https://origin-1.com",
      /*has_origin_wildcard=*/false, "/x", time_3_);
  // Profile_2_ did not add web_app_1.
  url_handler_prefs::ResetSavedChoice(
      LocalState(), web_app_1->app_id(), profile_2_, "https://origin-1.com",
      /*has_origin_wildcard=*/false, "/a", time_3_);
  // Profile_1_ does not target origin-2.com.
  url_handler_prefs::ResetSavedChoice(
      LocalState(), web_app_1->app_id(), profile_1_, "https://origin-2.com",
      /*has_origin_wildcard=*/false, "/a", time_3_);
  //  Neither app was added with origin_wildcard.
  url_handler_prefs::ResetSavedChoice(
      LocalState(), web_app_1->app_id(), profile_1_, "https://origin-1.com",
      /*has_origin_wildcard=*/true, "/a", time_3_);

  // Prefs should remain the same after invalid resets.
  ExpectUrlHandlerPrefs(prefs_before_invalid_resets);

  // Reset with valid input.
  url_handler_prefs::ResetSavedChoice(
      LocalState(), web_app_1->app_id(), profile_1_, "https://origin-1.com",
      /*has_origin_wildcard=*/false, "/a", time_3_);

  // Choice for /a is reset.
  // Choice for /b is unchanged.
  // Choice for https://origin-2.com/* is unchanged.
  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 1,
        "path": "/a",
        "timestamp": "12591331200000000"
      },
      {
        "choice": 2,
        "path": "/b",
        "timestamp": "12591244800000000"
      }],
      "profile_path": "/profile1"
    } ],
    "https://origin-2.com": [ {
      "app_id": "dioomdeompgjpnegoidgaopfdnbbljlb",
      "exclude_paths": [ ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 2,
        "path": "/*",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile2"
    } ]
  })");
}

TEST_F(UrlHandlerPrefsTest, ResetSavedChoice_InBrowser) {
  const auto web_app_1 =
      WebAppWithUrlHandlers(app_url_1_, {apps::UrlHandlerInfo(origin_1_)});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers(), time_1_);
  // Save choice as UrlHandlerSavedChoice::kInBrowser.
  url_handler_prefs::SaveOpenInBrowser(LocalState(), origin_url_1_, time_2_);
  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 0,
        "path": "/*",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");

  // Reset with valid input.
  url_handler_prefs::ResetSavedChoice(LocalState(), /*app_id=*/absl::nullopt,
                                      profile_1_, "https://origin-1.com",
                                      /*has_origin_wildcard=*/false, "/*",
                                      time_3_);
  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 1,
        "path": "/*",
        "timestamp": "12591331200000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");
}

TEST_F(UrlHandlerPrefsTest, ResetSavedChoice_OriginWildcardInApp) {
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_,
      {apps::UrlHandlerInfo(origin_1_, /*has_origin_wildcard=*/true, {}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers(), time_1_);
  // Save choice as UrlHandlerSavedChoice::kInBrowser.
  url_handler_prefs::SaveOpenInApp(LocalState(), web_app_1->app_id(),
                                   profile_1_, origin_url_1_, time_2_);
  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ ],
      "has_origin_wildcard": true,
      "include_paths": [ {
        "choice": 2,
        "path": "/*",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");

  // Reset with valid input.
  url_handler_prefs::ResetSavedChoice(LocalState(), /*app_id=*/absl::nullopt,
                                      profile_1_, "https://origin-1.com",
                                      /*has_origin_wildcard=*/true, "/*",
                                      time_3_);
  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ ],
      "has_origin_wildcard": true,
      "include_paths": [ {
        "choice": 1,
        "path": "/*",
        "timestamp": "12591331200000000"
      } ],
      "profile_path": "/profile1"
    } ]
  })");
}

TEST_F(UrlHandlerPrefsTest, ResetSavedChoice_InBrowserInMultipleApps) {
  const auto web_app_1 = WebAppWithUrlHandlers(
      app_url_1_, {apps::UrlHandlerInfo(origin_1_, false, {"/a", "/b"}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app_1->app_id(), profile_1_,
                               web_app_1->url_handlers(), time_1_);
  const auto web_app_2 = WebAppWithUrlHandlers(
      app_url_2_, {apps::UrlHandlerInfo(origin_1_, false, {"/a"}, {})});
  url_handler_prefs::AddWebApp(LocalState(), web_app_2->app_id(), profile_2_,
                               web_app_2->url_handlers(), time_1_);
  // Save all paths as UrlHandlerSavedChoice::kInApp.
  url_handler_prefs::SaveOpenInBrowser(LocalState(),
                                       origin_url_1_.Resolve("/a"), time_2_);
  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 0,
        "path": "/a",
        "timestamp": "12591244800000000"
      },
      {
        "choice": 1,
        "path": "/b",
        "timestamp": "12591158400000000"
      } ],
      "profile_path": "/profile1"
    },
    {
      "app_id": "dioomdeompgjpnegoidgaopfdnbbljlb",
      "exclude_paths": [ ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 0,
        "path": "/a",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile2"
    } ]
  })");

  url_handler_prefs::ResetSavedChoice(LocalState(), absl::nullopt, profile_1_,
                                      "https://origin-1.com", false, "/a",
                                      time_3_);

  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 1,
        "path": "/a",
        "timestamp": "12591331200000000"
      },
      {
        "choice": 1,
        "path": "/b",
        "timestamp": "12591158400000000"
      } ],
      "profile_path": "/profile1"
    },
    {
      "app_id": "dioomdeompgjpnegoidgaopfdnbbljlb",
      "exclude_paths": [ ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 0,
        "path": "/a",
        "timestamp": "12591244800000000"
      } ],
      "profile_path": "/profile2"
    } ]
  })");

  url_handler_prefs::ResetSavedChoice(LocalState(), absl::nullopt, profile_2_,
                                      "https://origin-1.com", false, "/a",
                                      time_3_);
  ExpectUrlHandlerPrefs(R"({
    "https://origin-1.com": [ {
      "app_id": "hfbpnmjjjooicehokhgjihcnkmbbpefl",
      "exclude_paths": [ ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 1,
        "path": "/a",
        "timestamp": "12591331200000000"
      },
      {
        "choice": 1,
        "path": "/b",
        "timestamp": "12591158400000000"
      } ],
      "profile_path": "/profile1"
    },
    {
      "app_id": "dioomdeompgjpnegoidgaopfdnbbljlb",
      "exclude_paths": [ ],
      "has_origin_wildcard": false,
      "include_paths": [ {
        "choice": 1,
        "path": "/a",
        "timestamp": "12591331200000000"
      } ],
      "profile_path": "/profile2"
    } ]
  })");
}

}  // namespace web_app
