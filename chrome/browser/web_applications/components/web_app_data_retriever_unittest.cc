// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_data_retriever.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/installable/fake_installable_manager.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/common/chrome_render_frame.mojom-test-utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest.h"

namespace web_app {

namespace {

const GURL kFooUrl("https://foo.example");
const GURL kFooUrl2("https://foo.example/bar");
const char kFooTitle[] = "Foo Title";
const GURL kBarUrl("https://bar.example");

}  // namespace

class FakeChromeRenderFrame
    : public chrome::mojom::ChromeRenderFrameInterceptorForTesting {
 public:
  FakeChromeRenderFrame() = default;
  ~FakeChromeRenderFrame() override = default;

  ChromeRenderFrame* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<ChromeRenderFrame>(std::move(handle)));
  }

  // Set |web_app_info| to respond on |GetWebApplicationInfo|.
  void SetWebApplicationInfo(const WebApplicationInfo& web_app_info) {
    web_app_info_ = web_app_info;
  }

  void GetWebApplicationInfo(GetWebApplicationInfoCallback callback) override {
    std::move(callback).Run(web_app_info_);
  }

 private:
  WebApplicationInfo web_app_info_;

  mojo::AssociatedReceiver<chrome::mojom::ChromeRenderFrame> receiver_{this};
};

class WebAppDataRetrieverTest : public ChromeRenderViewHostTestHarness {
 public:
  WebAppDataRetrieverTest() = default;
  ~WebAppDataRetrieverTest() override = default;

  // Set fake ChromeRenderFrame to avoid mojo connection errors.
  void SetFakeChromeRenderFrame() {
    web_contents()
        ->GetMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            chrome::mojom::ChromeRenderFrame::Name_,
            base::BindRepeating(&FakeChromeRenderFrame::Bind,
                                base::Unretained(&fake_chrome_render_frame_)));
  }

  void SetRendererWebApplicationInfo(const WebApplicationInfo& web_app_info) {
    fake_chrome_render_frame_.SetWebApplicationInfo(web_app_info);
  }

  void GetWebApplicationInfoCallback(
      base::OnceClosure quit_closure,
      std::unique_ptr<WebApplicationInfo> web_app_info) {
    web_app_info_ = std::move(web_app_info);
    std::move(quit_closure).Run();
  }

  void GetIconsCallback(base::OnceClosure quit_closure,
                        std::vector<WebApplicationIconInfo> icons) {
    icons_ = std::move(icons);
    std::move(quit_closure).Run();
  }

  std::unique_ptr<WebApplicationInfo> CreateWebApplicationInfo(
      const GURL& url,
      const std::string name,
      const std::string description,
      const GURL& scope,
      base::Optional<SkColor> theme_color) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();

    web_app_info->app_url = url;
    web_app_info->title = base::UTF8ToUTF16(name);
    web_app_info->description = base::UTF8ToUTF16(description);
    web_app_info->scope = scope;
    web_app_info->theme_color = theme_color;

    return web_app_info;
  }

  static base::NullableString16 ToNullableUTF16(const std::string& str) {
    return base::NullableString16(base::UTF8ToUTF16(str), false);
  }

 protected:
  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  const std::unique_ptr<WebApplicationInfo>& web_app_info() {
    return web_app_info_.value();
  }

  const std::vector<WebApplicationIconInfo>& icons() { return icons_; }

 private:
  FakeChromeRenderFrame fake_chrome_render_frame_;
  base::Optional<std::unique_ptr<WebApplicationInfo>> web_app_info_;
  std::vector<WebApplicationIconInfo> icons_;

  DISALLOW_COPY_AND_ASSIGN(WebAppDataRetrieverTest);
};

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_NoEntry) {
  SetFakeChromeRenderFrame();

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(nullptr, web_app_info());
}

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_AppUrlAbsent) {
  SetFakeChromeRenderFrame();

  web_contents_tester()->NavigateAndCommit(kFooUrl);

  WebApplicationInfo original_web_app_info;
  original_web_app_info.app_url = GURL();

  SetRendererWebApplicationInfo(original_web_app_info);

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // If the WebApplicationInfo has no URL, we fallback to the last committed
  // URL.
  EXPECT_EQ(kFooUrl, web_app_info()->app_url);
}

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_AppUrlPresent) {
  SetFakeChromeRenderFrame();

  web_contents_tester()->NavigateAndCommit(kFooUrl);

  WebApplicationInfo original_web_app_info;
  original_web_app_info.app_url = kBarUrl;

  SetRendererWebApplicationInfo(original_web_app_info);

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(original_web_app_info.app_url, web_app_info()->app_url);
}

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_TitleAbsentFromRenderer) {
  SetFakeChromeRenderFrame();

  web_contents_tester()->NavigateAndCommit(kFooUrl);

  const auto web_contents_title = base::UTF8ToUTF16(kFooTitle);
  web_contents_tester()->SetTitle(web_contents_title);

  WebApplicationInfo original_web_app_info;
  original_web_app_info.title = base::UTF8ToUTF16("");

  SetRendererWebApplicationInfo(original_web_app_info);

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // If the WebApplicationInfo has no title, we fallback to the WebContents
  // title.
  EXPECT_EQ(web_contents_title, web_app_info()->title);
}

TEST_F(WebAppDataRetrieverTest,
       GetWebApplicationInfo_TitleAbsentFromWebContents) {
  SetFakeChromeRenderFrame();

  web_contents_tester()->NavigateAndCommit(kFooUrl);

  web_contents_tester()->SetTitle(base::UTF8ToUTF16(""));

  WebApplicationInfo original_web_app_info;
  original_web_app_info.title = base::UTF8ToUTF16("");

  SetRendererWebApplicationInfo(original_web_app_info);

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // If the WebApplicationInfo has no title and the WebContents has no title,
  // we fallback to app_url.
  EXPECT_EQ(base::UTF8ToUTF16(web_app_info()->app_url.spec()),
            web_app_info()->title);
}

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_ConnectionError) {
  // Do not set fake ChromeRenderFrame to simulate connection error.

  web_contents_tester()->NavigateAndCommit(kFooUrl);

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(nullptr, web_app_info());
}

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_WebContentsDestroyed) {
  SetFakeChromeRenderFrame();

  web_contents_tester()->NavigateAndCommit(kFooUrl);

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  DeleteContents();
  run_loop.Run();

  EXPECT_EQ(nullptr, web_app_info());
}

TEST_F(WebAppDataRetrieverTest,
       CheckInstallabilityAndRetrieveManifest_WebContentsDestroyed) {
  SetFakeChromeRenderFrame();

  web_contents_tester()->NavigateAndCommit(kFooUrl);

  {
    auto manifest = std::make_unique<blink::Manifest>();
    FakeInstallableManager::CreateForWebContentsWithManifest(
        web_contents(), NO_MANIFEST, GURL(), std::move(manifest));
  }

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.CheckInstallabilityAndRetrieveManifest(
      web_contents(), /*bypass_service_worker_check=*/false,
      base::BindLambdaForTesting([&](base::Optional<blink::Manifest> manifest,
                                     bool valid_manifest_for_web_app,
                                     bool is_installable) {
        EXPECT_FALSE(manifest);
        EXPECT_FALSE(valid_manifest_for_web_app);
        EXPECT_FALSE(is_installable);
        run_loop.Quit();
      }));
  DeleteContents();
  run_loop.Run();
}

TEST_F(WebAppDataRetrieverTest, GetIcons_WebContentsDestroyed) {
  SetFakeChromeRenderFrame();

  web_contents_tester()->NavigateAndCommit(kFooUrl);

  const std::vector<GURL> icon_urls;
  bool skip_page_favicons = true;

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetIcons(web_contents(), icon_urls, skip_page_favicons,
                     WebAppIconDownloader::Histogram::kForCreate,
                     base::BindLambdaForTesting([&](IconsMap icons_map) {
                       EXPECT_TRUE(icons_map.empty());
                       run_loop.Quit();
                     }));
  DeleteContents();
  run_loop.Run();
}

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_FrameNavigated) {
  SetFakeChromeRenderFrame();

  web_contents_tester()->NavigateAndCommit(kFooUrl);

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  web_contents_tester()->NavigateAndCommit(kFooUrl2);
  run_loop.Run();

  EXPECT_EQ(nullptr, web_app_info());
}

TEST_F(WebAppDataRetrieverTest, CheckInstallabilityAndRetrieveManifest) {
  SetFakeChromeRenderFrame();

  const GURL manifest_start_url = GURL("https://example.com/start");
  const std::string manifest_short_name = "Short Name from Manifest";
  const std::string manifest_name = "Name from Manifest";
  const GURL manifest_scope = GURL("https://example.com/scope");
  const base::Optional<SkColor> manifest_theme_color = 0xAABBCCDD;

  {
    auto manifest = std::make_unique<blink::Manifest>();
    manifest->short_name = ToNullableUTF16(manifest_short_name);
    manifest->name = ToNullableUTF16(manifest_name);
    manifest->start_url = manifest_start_url;
    manifest->scope = manifest_scope;
    manifest->theme_color = manifest_theme_color;

    FakeInstallableManager::CreateForWebContentsWithManifest(
        web_contents(), NO_ERROR_DETECTED, GURL("https://example.com/manifest"),
        std::move(manifest));
  }

  base::RunLoop run_loop;
  bool callback_called = false;

  WebAppDataRetriever retriever;

  retriever.CheckInstallabilityAndRetrieveManifest(
      web_contents(), /*bypass_service_worker_check=*/false,
      base::BindLambdaForTesting([&](base::Optional<blink::Manifest> result,
                                     bool valid_manifest_for_web_app,
                                     bool is_installable) {
        EXPECT_TRUE(is_installable);

        EXPECT_EQ(base::UTF8ToUTF16(manifest_short_name),
                  result->short_name.string());
        EXPECT_EQ(base::UTF8ToUTF16(manifest_name), result->name.string());
        EXPECT_EQ(manifest_start_url, result->start_url);
        EXPECT_EQ(manifest_scope, result->scope);
        EXPECT_EQ(manifest_theme_color, result->theme_color);

        callback_called = true;
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(WebAppDataRetrieverTest, CheckInstallabilityFails) {
  SetFakeChromeRenderFrame();

  {
    auto manifest = std::make_unique<blink::Manifest>();
    FakeInstallableManager::CreateForWebContentsWithManifest(
        web_contents(), NO_MANIFEST, GURL(), std::move(manifest));
  }

  base::RunLoop run_loop;
  bool callback_called = false;

  WebAppDataRetriever retriever;

  retriever.CheckInstallabilityAndRetrieveManifest(
      web_contents(), /*bypass_service_worker_check=*/false,
      base::BindLambdaForTesting([&](base::Optional<blink::Manifest> result,
                                     bool valid_manifest_for_web_app,
                                     bool is_installable) {
        EXPECT_FALSE(is_installable);
        EXPECT_FALSE(valid_manifest_for_web_app);
        callback_called = true;
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

}  // namespace web_app
