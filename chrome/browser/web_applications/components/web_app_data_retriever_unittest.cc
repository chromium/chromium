// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_data_retriever.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/constants.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace web_app {

namespace {

const char kFooUrl[] = "https://foo.example";
const char kFooUrl2[] = "https://foo.example/bar";
const char kFooTitle[] = "Foo Title";
const char kBarUrl[] = "https://bar.example";

constexpr int kIconSizesToGenerate[] = {
    extension_misc::EXTENSION_ICON_SMALL,
    extension_misc::EXTENSION_ICON_SMALL * 2,
    extension_misc::EXTENSION_ICON_MEDIUM,
    extension_misc::EXTENSION_ICON_MEDIUM * 2,
    extension_misc::EXTENSION_ICON_LARGE,
    extension_misc::EXTENSION_ICON_LARGE * 2,
};

}  // namespace

class FakeChromeRenderFrame
    : public chrome::mojom::ChromeRenderFrameInterceptorForTesting {
 public:
  explicit FakeChromeRenderFrame(const WebApplicationInfo& web_app_info)
      : web_app_info_(web_app_info) {}
  ~FakeChromeRenderFrame() override = default;

  ChromeRenderFrame* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    binding_.Bind(
        mojo::AssociatedInterfaceRequest<ChromeRenderFrame>(std::move(handle)));
  }

  void GetWebApplicationInfo(GetWebApplicationInfoCallback callback) override {
    std::move(callback).Run(web_app_info_);
  }

 private:
  WebApplicationInfo web_app_info_;

  mojo::AssociatedBinding<chrome::mojom::ChromeRenderFrame> binding_{this};
};

class WebAppDataRetrieverTest : public ChromeRenderViewHostTestHarness {
 public:
  WebAppDataRetrieverTest() = default;
  ~WebAppDataRetrieverTest() override = default;

  void SetFakeChromeRenderFrame(
      FakeChromeRenderFrame* fake_chrome_render_frame) {
    web_contents()
        ->GetMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            chrome::mojom::ChromeRenderFrame::Name_,
            base::BindRepeating(&FakeChromeRenderFrame::Bind,
                                base::Unretained(fake_chrome_render_frame)));
  }

  void GetWebApplicationInfoCallback(
      base::OnceClosure quit_closure,
      std::unique_ptr<WebApplicationInfo> web_app_info) {
    web_app_info_ = std::move(web_app_info);
    std::move(quit_closure).Run();
  }

  void GetIconsCallback(base::OnceClosure quit_closure,
                        std::vector<WebApplicationInfo::IconInfo> icons) {
    icons_ = std::move(icons);
    std::move(quit_closure).Run();
  }

 protected:
  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  const std::unique_ptr<WebApplicationInfo>& web_app_info() {
    return web_app_info_.value();
  }

  const std::vector<WebApplicationInfo::IconInfo>& icons() { return icons_; }

 private:
  base::Optional<std::unique_ptr<WebApplicationInfo>> web_app_info_;
  std::vector<WebApplicationInfo::IconInfo> icons_;

  DISALLOW_COPY_AND_ASSIGN(WebAppDataRetrieverTest);
};

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_NoEntry) {
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
  web_contents_tester()->NavigateAndCommit(GURL(kFooUrl));

  WebApplicationInfo original_web_app_info;
  original_web_app_info.app_url = GURL();

  FakeChromeRenderFrame fake_chrome_render_frame(original_web_app_info);
  SetFakeChromeRenderFrame(&fake_chrome_render_frame);

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // If the WebApplicationInfo has no URL, we fallback to the last committed
  // URL.
  EXPECT_EQ(GURL(kFooUrl), web_app_info()->app_url);
}

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_AppUrlPresent) {
  web_contents_tester()->NavigateAndCommit(GURL(kFooUrl));

  WebApplicationInfo original_web_app_info;
  original_web_app_info.app_url = GURL(kBarUrl);

  FakeChromeRenderFrame fake_chrome_render_frame(original_web_app_info);
  SetFakeChromeRenderFrame(&fake_chrome_render_frame);

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
  web_contents_tester()->NavigateAndCommit(GURL(kFooUrl));

  const auto web_contents_title = base::UTF8ToUTF16(kFooTitle);
  web_contents_tester()->SetTitle(web_contents_title);

  WebApplicationInfo original_web_app_info;
  original_web_app_info.title = base::UTF8ToUTF16("");

  FakeChromeRenderFrame fake_chrome_render_frame(original_web_app_info);
  SetFakeChromeRenderFrame(&fake_chrome_render_frame);

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
  web_contents_tester()->NavigateAndCommit(GURL(kFooUrl));

  web_contents_tester()->SetTitle(base::UTF8ToUTF16(""));

  WebApplicationInfo original_web_app_info;
  original_web_app_info.title = base::UTF8ToUTF16("");

  FakeChromeRenderFrame fake_chrome_render_frame(original_web_app_info);
  SetFakeChromeRenderFrame(&fake_chrome_render_frame);

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

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_WebContentsDestroyed) {
  web_contents_tester()->NavigateAndCommit(GURL(kFooUrl));

  FakeChromeRenderFrame fake_chrome_render_frame{WebApplicationInfo()};
  SetFakeChromeRenderFrame(&fake_chrome_render_frame);

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

TEST_F(WebAppDataRetrieverTest, GetWebApplicationInfo_FrameNavigated) {
  web_contents_tester()->NavigateAndCommit(GURL(kFooUrl));

  FakeChromeRenderFrame fake_chrome_render_frame{WebApplicationInfo()};
  SetFakeChromeRenderFrame(&fake_chrome_render_frame);

  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppDataRetrieverTest::GetWebApplicationInfoCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  web_contents_tester()->NavigateAndCommit(GURL(kFooUrl2));
  run_loop.Run();

  EXPECT_EQ(nullptr, web_app_info());
}

TEST_F(WebAppDataRetrieverTest, GetIcons_NoIconsProvided) {
  base::RunLoop run_loop;
  WebAppDataRetriever retriever;
  retriever.GetIcons(
      GURL(kFooUrl), std::vector<GURL>(),
      base::BindOnce(&WebAppDataRetrieverTest::GetIconsCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // Make sure that icons have been generated for all sizes.
  for (int size : kIconSizesToGenerate) {
    int generated_icons_for_size =
        std::count_if(icons().begin(), icons().end(),
                      [&size](const WebApplicationInfo::IconInfo& icon) {
                        return icon.width == size && icon.height == size;
                      });
    EXPECT_EQ(1, generated_icons_for_size);
  }

  for (const auto& icon : icons()) {
    EXPECT_FALSE(icon.data.drawsNothing());
    // Since all icons are generated, they should have an empty url.
    EXPECT_TRUE(icon.url.is_empty());
  }
}

}  // namespace web_app
