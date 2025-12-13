// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/commands/manifest_silent_update_command.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_scope.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

class WebAppScopeExtensionsTest : public WebAppTest {
 public:
  WebAppScopeExtensionsTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kWebAppPredictableAppUpdating,
                              features::kWebAppUsePrimaryIcon},
        /*disabled_features=*/{});
  }
  ~WebAppScopeExtensionsTest() override = default;

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
    SetupPageNoScopeExtensions(scope);
    return test::InstallForWebContents(
        profile(), web_contents(),
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  }

  webapps::AppId InstallWebAppWithScopeExtensions(
      const GURL& scope,
      std::vector<blink::mojom::ManifestScopeExtensionPtr> scope_extensions) {
    SetupPageWithScopeExtensions(scope, std::move(scope_extensions));
    return test::InstallForWebContents(
        profile(), web_contents(),
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  void SetupPageNoScopeExtensions(const GURL& url) {
    web_contents_manager().SetUrlLoaded(web_contents(), url);
    web_contents_manager().CreateBasicInstallPageState(
        /*install_url=*/url,
        /*manifest_url=*/GURL("https://www.example.com/manifest.json"),
        /*start_url=*/url);
  }

  void SetupPageWithScopeExtensions(
      const GURL& url,
      std::vector<blink::mojom::ManifestScopeExtensionPtr> scope_extensions) {
    SetupPageNoScopeExtensions(url);
    auto& page_state = web_contents_manager().GetOrCreatePageState(url);
    page_state.manifest_before_default_processing->scope_extensions =
        std::move(scope_extensions);
  }

  WebAppRegistrar& registrar() { return fake_provider().registrar_unsafe(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(WebAppScopeExtensionsTest, TestScopeNotifiedOnReinstall) {
  const GURL scope("https://example.com/");
  webapps::AppId app_id = InstallWebAppWithScope(scope);

  // Re-install the app with extended scope, and verify that the scope observer
  // is notified.
  WebAppTestRegistryObserverAdapter adapter(
      &fake_provider().registrar_unsafe());
  base::test::TestFuture<const webapps::AppId&, const WebAppScope&> future;
  adapter.SetWebAppEffectiveScopeChangedDelegate(future.GetRepeatingCallback());
  std::vector<blink::mojom::ManifestScopeExtensionPtr> scope_extensions;
  scope_extensions.push_back(blink::mojom::ManifestScopeExtension::New(
      url::Origin::Create(GURL("https://example.org")),
      /*has_origin_wildcard=*/false));
  scope_extensions.push_back(blink::mojom::ManifestScopeExtension::New(
      url::Origin::Create(GURL("https://example.net")),
      /*has_origin_wildcard=*/true));
  InstallWebAppWithScopeExtensions(scope, std::move(scope_extensions));

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(app_id, future.Get<webapps::AppId>());

  // Now re-install without the scope extensions, and verify that the scope
  // observer is notified.
  future.Clear();
  InstallWebAppWithScope(scope);
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(app_id, future.Get<webapps::AppId>());
}

TEST_F(WebAppScopeExtensionsTest, TestScopeNotifiedOnAddedUpdate) {
  const GURL scope("https://example.com/");
  webapps::AppId app_id = InstallWebAppWithScope(scope);

  // Update the app with extended scope, and verify that the scope observer
  // is notified.
  WebAppTestRegistryObserverAdapter adapter(
      &fake_provider().registrar_unsafe());
  base::test::TestFuture<const webapps::AppId&, const WebAppScope&> future;
  adapter.SetWebAppEffectiveScopeChangedDelegate(future.GetRepeatingCallback());

  // Do the update.
  std::vector<blink::mojom::ManifestScopeExtensionPtr> scope_extensions;
  scope_extensions.push_back(blink::mojom::ManifestScopeExtension::New(
      url::Origin::Create(GURL("https://example.org")),
      /*has_origin_wildcard=*/false));
  scope_extensions.push_back(blink::mojom::ManifestScopeExtension::New(
      url::Origin::Create(GURL("https://example.net")),
      /*has_origin_wildcard=*/true));
  SetupPageWithScopeExtensions(scope, std::move(scope_extensions));
  base::test::TestFuture<ManifestSilentUpdateCompletionInfo>
      manifest_silent_update_future;
  fake_provider().scheduler().ScheduleManifestSilentUpdate(
      *web_contents(), /*previous_time_for_silent_icon_update=*/std::nullopt,
      manifest_silent_update_future.GetCallback());
  ASSERT_TRUE(manifest_silent_update_future.Wait());
  EXPECT_EQ(manifest_silent_update_future.Take().result,
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);

  // Wait for the scope change to be observed.
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(app_id, future.Get<webapps::AppId>());
  // Double check the scope extensions are there.
  WebAppScope effective_scope = future.Get<WebAppScope>();
  EXPECT_TRUE(effective_scope.IsInScope(GURL("https://example.org")));
  EXPECT_TRUE(effective_scope.IsInScope(GURL("https://example.net")));
  EXPECT_TRUE(effective_scope.IsInScope(GURL("https://sub.example.net")));
}

TEST_F(WebAppScopeExtensionsTest, TestScopeNotifiedOnRemovedUpdate) {
  const GURL scope("https://example.com/");
  std::vector<blink::mojom::ManifestScopeExtensionPtr> scope_extensions;
  scope_extensions.push_back(blink::mojom::ManifestScopeExtension::New(
      url::Origin::Create(GURL("https://example.org")),
      /*has_origin_wildcard=*/false));
  scope_extensions.push_back(blink::mojom::ManifestScopeExtension::New(
      url::Origin::Create(GURL("https://example.net")),
      /*has_origin_wildcard=*/true));
  const webapps::AppId app_id =
      InstallWebAppWithScopeExtensions(scope, std::move(scope_extensions));

  // Update the app with no extensions, and verify that the scope observer
  // is notified.
  WebAppTestRegistryObserverAdapter adapter(
      &fake_provider().registrar_unsafe());
  base::test::TestFuture<const webapps::AppId&, const WebAppScope&> future;
  adapter.SetWebAppEffectiveScopeChangedDelegate(future.GetRepeatingCallback());

  // Do the update.
  SetupPageNoScopeExtensions(scope);
  base::test::TestFuture<ManifestSilentUpdateCompletionInfo>
      manifest_silent_update_future;
  fake_provider().scheduler().ScheduleManifestSilentUpdate(
      *web_contents(), /*previous_time_for_silent_icon_update=*/std::nullopt,
      manifest_silent_update_future.GetCallback());
  ASSERT_TRUE(manifest_silent_update_future.Wait());
  EXPECT_EQ(manifest_silent_update_future.Take().result,
            ManifestSilentUpdateCheckResult::kAppSilentlyUpdated);

  // Wait for the scope change to be observed.
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(app_id, future.Get<webapps::AppId>());
  // Double check the scope extensions are gone.
  WebAppScope effective_scope = future.Get<WebAppScope>();
  EXPECT_FALSE(effective_scope.IsInScope(GURL("https://example.org")));
  EXPECT_FALSE(effective_scope.IsInScope(GURL("https://example.net")));
  EXPECT_FALSE(effective_scope.IsInScope(GURL("https://sub.example.net")));
}

}  // namespace

}  // namespace web_app
