// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/browser/installable/fake_installable_manager.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "components/webapps/common/web_page_metadata_agent.mojom-test-utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

namespace {

const char16_t kFooTitle[] = u"Foo Title";

}  // namespace

class FakeWebPageMetadataAgent
    : public webapps::mojom::WebPageMetadataAgentInterceptorForTesting {
 public:
  FakeWebPageMetadataAgent() = default;
  ~FakeWebPageMetadataAgent() override = default;

  WebPageMetadataAgent* GetForwardingInterface() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<WebPageMetadataAgent>(
        std::move(handle)));
  }

  void SetWebPageMetadata(const GURL& application_url,
                          const std::u16string& title,
                          const std::u16string& description) {
    web_page_metadata_->application_name = title;
    web_page_metadata_->description = description;
    web_page_metadata_->application_url = application_url;
  }

  void GetWebPageMetadata(GetWebPageMetadataCallback callback) override {
    std::move(callback).Run(web_page_metadata_.Clone());
  }

 private:
  webapps::mojom::WebPageMetadataPtr web_page_metadata_ =
      webapps::mojom::WebPageMetadata::New();

  mojo::AssociatedReceiver<webapps::mojom::WebPageMetadataAgent> receiver_{
      this};
};

class WebAppDataRetrieverTest : public ChromeRenderViewHostTestHarness {
 public:
  WebAppDataRetrieverTest() = default;
  WebAppDataRetrieverTest(const WebAppDataRetrieverTest&) = delete;
  WebAppDataRetrieverTest& operator=(const WebAppDataRetrieverTest&) = delete;
  ~WebAppDataRetrieverTest() override = default;

  // Set fake WebPageMetadataAgent to avoid mojo connection errors.
  void SetFakeWebPageMetadataAgent() {
    web_contents()
        ->GetPrimaryMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            webapps::mojom::WebPageMetadataAgent::Name_,
            base::BindRepeating(&FakeWebPageMetadataAgent::Bind,
                                base::Unretained(&fake_chrome_render_frame_)));

    // When ProactivelySwapBrowsingInstance or RenderDocument is enabled on
    // same-site main-frame navigations, a same-site navigation might result in
    // a change of RenderFrames, which WebAppDataRetriever does not track (it
    // tracks the old RenderFrame where the navigation started in). So we
    // should disable same-site proactive BrowsingInstance for the main frame.
    // Note: this will not disable RenderDocument.
    // TODO(crbug.com/40615943): Make WebAppDataRetriever support a change of
    // RenderFrames.
    content::DisableProactiveBrowsingInstanceSwapFor(
        web_contents()->GetPrimaryMainFrame());
  }

  void SetRendererWebPageMetadata(const GURL& application_url,
                                  const std::u16string& title,
                                  const std::u16string& description) {
    fake_chrome_render_frame_.SetWebPageMetadata(application_url, title,
                                                 description);
  }

  void GetWebAppInstallInfoCallback(
      base::OnceClosure quit_closure,
      std::unique_ptr<WebAppInstallInfo> web_app_info) {
    web_app_info_ = std::move(web_app_info);
    std::move(quit_closure).Run();
  }

  void GetIconsCallback(base::OnceClosure quit_closure,
                        std::vector<apps::IconInfo> icons) {
    icons_ = std::move(icons);
    std::move(quit_closure).Run();
  }

 protected:
  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  const std::unique_ptr<WebAppInstallInfo>& web_app_info() {
    return web_app_info_.value();
  }

  const std::vector<apps::IconInfo>& icons() { return icons_; }

 private:
  FakeWebPageMetadataAgent fake_chrome_render_frame_;
  std::optional<std::unique_ptr<WebAppInstallInfo>> web_app_info_;
  std::vector<apps::IconInfo> icons_;
};

TEST_F(WebAppDataRetrieverTest, GetWebAppInstallInfo_NoEntry) {
  SetFakeWebPageMetadataAgent();

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebAppInstallInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebAppInstallInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(nullptr, web_app_info());
}

TEST_F(WebAppDataRetrieverTest, GetWebAppInstallInfo_AppUrlAbsent) {
  SetFakeWebPageMetadataAgent();

  const GURL kFooUrl("https://foo.example");
  web_contents_tester()->NavigateAndCommit(kFooUrl);

  // No install info present.
  SetRendererWebPageMetadata(/*application_url=*/GURL(), /*title=*/u"",
                             /*description=*/u"");

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebAppInstallInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebAppInstallInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // If the WebAppInstallInfo has no URL, we fallback to the last committed
  // URL.
  EXPECT_EQ(kFooUrl, web_app_info()->start_url());
}

TEST_F(WebAppDataRetrieverTest, GetWebAppInstallInfo_AppUrlPresent) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  GURL other_app_url = GURL("https://bar.example");
  std::u16string other_app_title = u"Other App Title";
  SetRendererWebPageMetadata(other_app_url, other_app_title,
                             /*description=*/u"");

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebAppInstallInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebAppInstallInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(other_app_url, web_app_info()->start_url());
  EXPECT_EQ(other_app_title, web_app_info()->title);
}

TEST_F(WebAppDataRetrieverTest, GetWebAppInstallInfo_TitleAbsentFromRenderer) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  web_contents_tester()->SetTitle(kFooTitle);

  SetRendererWebPageMetadata(GURL("https://foo.example"), /*title=*/u"",
                             /*description=*/u"");
  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebAppInstallInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebAppInstallInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // If the metadata has no title, we fallback to the WebContents title.
  EXPECT_EQ(kFooTitle, web_app_info()->title);
}

TEST_F(WebAppDataRetrieverTest,
       GetWebAppInstallInfo_TitleAbsentFromWebContents) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  web_contents_tester()->SetTitle(u"");

  SetRendererWebPageMetadata(GURL("https://foo.example"), /*title=*/u"",
                             /*description=*/u"");

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebAppInstallInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebAppInstallInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // If the WebAppInstallInfo has no title and the WebContents has no title, we
  // fallback to start_url.
  EXPECT_EQ(base::UTF8ToUTF16(web_app_info()->start_url().spec()),
            web_app_info()->title);
}

TEST_F(WebAppDataRetrieverTest, GetWebAppInstallInfo_ConnectionError) {
  // Do not set fake WebPageMetadataAgent to simulate connection error.

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebAppInstallInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebAppInstallInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(nullptr, web_app_info());
}

TEST_F(WebAppDataRetrieverTest, GetWebAppInstallInfo_WebContentsDestroyed) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebAppInstallInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebAppInstallInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  DeleteContents();
  run_loop.Run();

  EXPECT_EQ(nullptr, web_app_info());
}

TEST_F(WebAppDataRetrieverTest,
       CheckInstallabilityAndRetrieveManifest_WebContentsDestroyed) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  {
    webapps::FakeInstallableManager::CreateForWebContentsWithManifest(
        web_contents(), webapps::InstallableStatusCode::NO_MANIFEST, GURL(),
        blink::mojom::Manifest::New());
  }

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.CheckInstallabilityAndRetrieveManifest(
      web_contents(),
      base::BindLambdaForTesting(
          [&](blink::mojom::ManifestPtr opt_manifest,
              bool valid_manifest_for_web_app,
              webapps::InstallableStatusCode error_code) {
            EXPECT_FALSE(opt_manifest);
            EXPECT_FALSE(valid_manifest_for_web_app);
            EXPECT_EQ(error_code,
                      webapps::InstallableStatusCode::RENDERER_CANCELLED);
            run_loop.Quit();
          }));
  DeleteContents();
  run_loop.Run();
}

TEST_F(WebAppDataRetrieverTest, GetIcons_WebContentsDestroyed) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->NavigateAndCommit(GURL("https://foo.example"));

  bool skip_page_favicons = true;
  bool fail_all_if_any_fail = false;

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetIcons(web_contents(),
                     /*extra_favicon_urls=*/IconUrlSizeSet(),
                     skip_page_favicons, fail_all_if_any_fail,
                     base::BindLambdaForTesting(
                         [&](IconsDownloadedResult result, IconsMap icons_map,
                             DownloadedIconsHttpResults icons_http_results) {
                           EXPECT_TRUE(icons_map.empty());
                           run_loop.Quit();
                         }));
  DeleteContents();
  run_loop.Run();
}

TEST_F(WebAppDataRetrieverTest, GetWebAppInstallInfo_FrameNavigated) {
  SetFakeWebPageMetadataAgent();

  web_contents_tester()->SetTitle(kFooTitle);

  const GURL kFooUrl("https://foo.example/bar");
  web_contents_tester()->NavigateAndCommit(kFooUrl.DeprecatedGetOriginAsURL());

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebAppInstallInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebAppInstallInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  web_contents_tester()->NavigateAndCommit(kFooUrl);
  run_loop.Run();

  if (web_contents()
          ->GetPrimaryMainFrame()
          ->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    // If the RenderFrameHost changes, the FakeWebPageMetadataAgent mojo
    // connection will be disconnected, causing the callback to be called with
    // a null info.
    EXPECT_EQ(nullptr, web_app_info());
  } else {
    // Otherwise, the mojo connection will persist and the callback will get
    // the info from the previous document.
    EXPECT_EQ(kFooUrl.DeprecatedGetOriginAsURL(), web_app_info()->start_url());
    EXPECT_EQ(kFooTitle, web_app_info()->title);
  }
}

TEST_F(WebAppDataRetrieverTest, CheckInstallabilityAndRetrieveManifest) {
  SetFakeWebPageMetadataAgent();

  const GURL manifest_start_url = GURL("https://example.com/start");
  const std::u16string manifest_short_name = u"Short Name from Manifest";
  const std::u16string manifest_name = u"Name from Manifest";
  const GURL manifest_scope = GURL("https://example.com/scope");
  const SkColor manifest_theme_color = 0xAABBCCDD;

  {
    auto manifest = blink::mojom::Manifest::New();
    manifest->manifest_url = GURL("https://example.com/manifest");
    manifest->short_name = manifest_short_name;
    manifest->name = manifest_name;
    manifest->start_url = manifest_start_url;
    manifest->id = GenerateManifestIdFromStartUrlOnly(manifest_start_url);
    manifest->scope = manifest_scope;
    manifest->has_theme_color = true;
    manifest->theme_color = manifest_theme_color;

    webapps::FakeInstallableManager::CreateForWebContentsWithManifest(
        web_contents(), webapps::InstallableStatusCode::NO_ERROR_DETECTED,
        GURL("https://example.com/manifest"), std::move(manifest));
  }

  base::RunLoop run_loop;
  bool callback_called = false;

  WebAppDataRetriever retriever;

  retriever.CheckInstallabilityAndRetrieveManifest(
      web_contents(),
      base::BindLambdaForTesting(
          [&](blink::mojom::ManifestPtr opt_manifest,
              bool valid_manifest_for_web_app,
              webapps::InstallableStatusCode error_code) {
            EXPECT_EQ(error_code,
                      webapps::InstallableStatusCode::NO_ERROR_DETECTED);

            EXPECT_EQ(manifest_short_name, opt_manifest->short_name);
            EXPECT_EQ(manifest_name, opt_manifest->name);
            EXPECT_EQ(manifest_start_url, opt_manifest->start_url);
            EXPECT_EQ(manifest_scope, opt_manifest->scope);
            EXPECT_EQ(manifest_theme_color, opt_manifest->theme_color);
            EXPECT_EQ(GURL("https://example.com/manifest"),
                      opt_manifest->manifest_url);

            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(WebAppDataRetrieverTest, CheckInstallabilityFails) {
  SetFakeWebPageMetadataAgent();

  {
    webapps::FakeInstallableManager::CreateForWebContentsWithManifest(
        web_contents(), webapps::InstallableStatusCode::NO_MANIFEST, GURL(),
        blink::mojom::Manifest::New());
  }

  base::RunLoop run_loop;
  bool callback_called = false;

  WebAppDataRetriever retriever;

  retriever.CheckInstallabilityAndRetrieveManifest(
      web_contents(),
      base::BindLambdaForTesting(
          [&](blink::mojom::ManifestPtr opt_manifest,
              bool valid_manifest_for_web_app,
              webapps::InstallableStatusCode error_code) {
            EXPECT_EQ(error_code, webapps::InstallableStatusCode::NO_MANIFEST);
            EXPECT_FALSE(valid_manifest_for_web_app);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

}  // namespace web_app
