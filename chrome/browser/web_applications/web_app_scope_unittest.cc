// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_scope.h"

#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

class WebAppScopeTest : public WebAppTest {
 public:
  WebAppScopeTest() = default;
  ~WebAppScopeTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    auto fake_association_manager =
        std::make_unique<FakeWebAppOriginAssociationManager>();
    fake_association_manager->set_pass_through(true);
    fake_provider().SetOriginAssociationManager(
        std::move(fake_association_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  webapps::AppId InstallWebAppWithScope(const GURL& scope) {
    std::unique_ptr<WebAppInstallInfo> web_app_info =
        WebAppInstallInfo::CreateForTesting(scope);
    return test::InstallWebApp(profile(), std::move(web_app_info));
  }

  webapps::AppId InstallWebAppWithScopeExtensions(
      const GURL& scope,
      const std::vector<ScopeExtensionInfo>& scope_extensions) {
    std::unique_ptr<WebAppInstallInfo> web_app_info =
        WebAppInstallInfo::CreateForTesting(scope);
    web_app_info->scope_extensions = base::flat_set<ScopeExtensionInfo>(
        scope_extensions.begin(), scope_extensions.end());
    return test::InstallWebApp(profile(), std::move(web_app_info));
  }

  WebAppRegistrar& registrar() { return fake_provider().registrar_unsafe(); }
};

TEST_F(WebAppScopeTest, IsInScopeBasic) {
  const GURL scope("https://example.com/");
  webapps::AppId app_id = InstallWebAppWithScope(scope);
  std::optional<WebAppScope> web_app_scope =
      registrar().GetEffectiveScope(app_id);
  ASSERT_TRUE(web_app_scope);

  EXPECT_TRUE(web_app_scope->IsInScope(GURL("https://example.com/")));
  EXPECT_TRUE(web_app_scope->IsInScope(GURL("https://example.com/path")));
  EXPECT_FALSE(web_app_scope->IsInScope(GURL("https://example.org/")));
}

TEST_F(WebAppScopeTest, IsInScopeWithSubdir) {
  const GURL scope("https://example.com/subdir/");
  webapps::AppId app_id = InstallWebAppWithScope(scope);
  std::optional<WebAppScope> web_app_scope =
      registrar().GetEffectiveScope(app_id);
  ASSERT_TRUE(web_app_scope);

  EXPECT_TRUE(web_app_scope->IsInScope(GURL("https://example.com/subdir/")));
  EXPECT_TRUE(
      web_app_scope->IsInScope(GURL("https://example.com/subdir/path")));
  EXPECT_FALSE(web_app_scope->IsInScope(GURL("https://example.com/")));
  EXPECT_FALSE(web_app_scope->IsInScope(GURL("https://example.com/path")));
}

TEST_F(WebAppScopeTest, TestScopeFragmentIgnored) {
  const GURL kStartUrl("https://www.foo.com/bar/index.html");
  const GURL kScopeWithQueryAndFragments =
      GURL("https://www.foo.com/bar/?query=abc#fragment");

  std::unique_ptr<WebAppInstallInfo> install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(kStartUrl);
  install_info->scope = kScopeWithQueryAndFragments;
  webapps::AppId app_id =
      test::InstallWebApp(profile(), std::move(install_info));

  EXPECT_TRUE(fake_provider().registrar_unsafe().IsUrlInAppScope(
      GURL("https://www.foo.com/bar/"), app_id));

  std::optional<WebAppScope> web_app_scope =
      registrar().GetEffectiveScope(app_id);
  ASSERT_TRUE(web_app_scope);
  EXPECT_TRUE(web_app_scope->IsInScope(GURL("https://www.foo.com/bar/")));
}

TEST_F(WebAppScopeTest, IsInScopeHttpToHttpsUpgrade) {
  const GURL scope("http://example.com/");
  webapps::AppId app_id = InstallWebAppWithScope(scope);
  std::optional<WebAppScope> web_app_scope =
      registrar().GetEffectiveScope(app_id);
  ASSERT_TRUE(web_app_scope);

  EXPECT_TRUE(web_app_scope->IsInScope(GURL("http://example.com/")));
  EXPECT_FALSE(web_app_scope->IsInScope(
      GURL("https://example.com/"),
      WebAppScopeOptions{.allow_http_to_https_upgrade = false}));
  EXPECT_TRUE(web_app_scope->IsInScope(
      GURL("https://example.com/"),
      WebAppScopeOptions{.allow_http_to_https_upgrade = true}));
}

TEST_F(WebAppScopeTest, IsInScopeWithScopeExtensions) {
  const GURL scope("https://example.com/");
  const std::vector<ScopeExtensionInfo> scope_extensions = {
      ScopeExtensionInfo::CreateForOrigin(
          url::Origin::Create(GURL("https://example.org"))),
      ScopeExtensionInfo::CreateForOrigin(
          url::Origin::Create(GURL("https://example.net")),
          /*has_origin_wildcard=*/true)};
  webapps::AppId app_id =
      InstallWebAppWithScopeExtensions(scope, scope_extensions);
  std::optional<WebAppScope> web_app_scope =
      registrar().GetEffectiveScope(app_id);
  ASSERT_TRUE(web_app_scope);

  EXPECT_TRUE(web_app_scope->IsInScope(GURL("https://example.com/")));
  EXPECT_TRUE(web_app_scope->IsInScope(GURL("https://example.org/")));
  EXPECT_TRUE(web_app_scope->IsInScope(GURL("https://sub.example.net/")));
  EXPECT_TRUE(web_app_scope->IsInScope(GURL("https://example.net/")));
  EXPECT_FALSE(web_app_scope->IsInScope(GURL("https://sub.example.org/")));
  EXPECT_FALSE(web_app_scope->IsInScope(GURL("https://another.com/")));
}

TEST_F(WebAppScopeTest, LongerScopeWins) {
  const GURL scope1("https://example.com/");
  const GURL scope2("https://example.com/app/");
  webapps::AppId app_id1 = InstallWebAppWithScope(scope1);
  webapps::AppId app_id2 = InstallWebAppWithScope(scope2);
  std::optional<WebAppScope> web_app_scope1 =
      registrar().GetEffectiveScope(app_id1);
  ASSERT_TRUE(web_app_scope1);
  std::optional<WebAppScope> web_app_scope2 =
      registrar().GetEffectiveScope(app_id2);
  ASSERT_TRUE(web_app_scope2);

  EXPECT_GT(
      web_app_scope2->GetScopeScore(GURL("https://example.com/app/index.html")),
      web_app_scope1->GetScopeScore(
          GURL("https://example.com/app/index.html")));
}

TEST_F(WebAppScopeTest, GetScopeScoreBasic) {
  const GURL scope("https://example.com/");
  webapps::AppId app_id = InstallWebAppWithScope(scope);
  std::optional<WebAppScope> web_app_scope =
      registrar().GetEffectiveScope(app_id);
  ASSERT_TRUE(web_app_scope);

  EXPECT_GT(web_app_scope->GetScopeScore(GURL("https://example.com/")), 0);
  EXPECT_GT(web_app_scope->GetScopeScore(GURL("https://example.com/path")), 0);
  EXPECT_EQ(web_app_scope->GetScopeScore(GURL("https://example.org/")), 0);
}

TEST_F(WebAppScopeTest, GetScopeScoreWithSubdir) {
  const GURL scope("https://example.com/subdir/");
  webapps::AppId app_id = InstallWebAppWithScope(scope);
  std::optional<WebAppScope> web_app_scope =
      registrar().GetEffectiveScope(app_id);
  ASSERT_TRUE(web_app_scope);

  EXPECT_GT(web_app_scope->GetScopeScore(GURL("https://example.com/subdir/")),
            0);
  EXPECT_GT(
      web_app_scope->GetScopeScore(GURL("https://example.com/subdir/path")), 0);
  EXPECT_EQ(web_app_scope->GetScopeScore(GURL("https://example.com/")), 0);
  EXPECT_EQ(web_app_scope->GetScopeScore(GURL("https://example.com/path")), 0);
}

TEST_F(WebAppScopeTest, GetScopeScoreWithScopeExtensions) {
  const GURL scope("https://example.com/");
  const std::vector<ScopeExtensionInfo> scope_extensions = {
      ScopeExtensionInfo::CreateForOrigin(
          url::Origin::Create(GURL("https://example.org"))),
      ScopeExtensionInfo::CreateForOrigin(
          url::Origin::Create(GURL("https://example.net")),
          /*has_origin_wildcard=*/true)};
  webapps::AppId app_id =
      InstallWebAppWithScopeExtensions(scope, scope_extensions);
  std::optional<WebAppScope> web_app_scope =
      registrar().GetEffectiveScope(app_id);
  ASSERT_TRUE(web_app_scope);

  EXPECT_GT(web_app_scope->GetScopeScore(GURL("https://example.com/")), 0);
  EXPECT_GT(web_app_scope->GetScopeScore(GURL("https://example.org/")), 0);
  EXPECT_GT(web_app_scope->GetScopeScore(GURL("https://sub.example.net/")), 0);
  EXPECT_EQ(web_app_scope->GetScopeScore(GURL("https://another.com/")), 0);
}

TEST_F(WebAppScopeTest, GetScopeScoreExcludeScopeExtensions) {
  const GURL scope("https://example.com/");
  const std::vector<ScopeExtensionInfo> scope_extensions = {
      ScopeExtensionInfo::CreateForOrigin(
          url::Origin::Create(GURL("https://example.org")))};
  webapps::AppId app_id =
      InstallWebAppWithScopeExtensions(scope, scope_extensions);
  std::optional<WebAppScope> web_app_scope =
      registrar().GetEffectiveScope(app_id);
  ASSERT_TRUE(web_app_scope);

  EXPECT_EQ(web_app_scope->GetScopeScore(
                GURL("https://example.org/"),
                WebAppScopeScoreOptions{.exclude_scope_extensions = true}),
            0);
  EXPECT_GT(web_app_scope->GetScopeScore(
                GURL("https://example.org/"),
                WebAppScopeScoreOptions{.exclude_scope_extensions = false}),
            0);
}

TEST_F(WebAppScopeTest, RegularScopeHasHigherScoreThanExtendedScope) {
  // App A has a regular scope.
  const GURL scope_a("https://example.com/app/");
  webapps::AppId app_id_a = InstallWebAppWithScope(scope_a);

  // App B has an extended scope that is longer than App A's regular scope.
  const GURL scope_b("https://another.com/");
  const std::vector<ScopeExtensionInfo> scope_extensions_b = {
      ScopeExtensionInfo::CreateForScope(
          GURL("https://example.com/app/longer/"))};
  webapps::AppId app_id_b =
      InstallWebAppWithScopeExtensions(scope_b, scope_extensions_b);

  std::optional<WebAppScope> web_app_scope_a =
      registrar().GetEffectiveScope(app_id_a);
  ASSERT_TRUE(web_app_scope_a);
  std::optional<WebAppScope> web_app_scope_b =
      registrar().GetEffectiveScope(app_id_b);
  ASSERT_TRUE(web_app_scope_b);

  const GURL url_in_both_scopes("https://example.com/app/longer/page.html");

  int score_a = web_app_scope_a->GetScopeScore(url_in_both_scopes);
  EXPECT_GT(score_a, 0);
  int score_b = web_app_scope_b->GetScopeScore(url_in_both_scopes);
  EXPECT_GT(score_b, 0);

  // score_a should be larger than score_b, even though the match for app b
  // would have been longer.
  EXPECT_GT(score_a, score_b);
}

}  // namespace

}  // namespace web_app
