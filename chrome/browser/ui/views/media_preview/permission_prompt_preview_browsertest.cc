// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/save_desktop_snapshot.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

const int kFixedPort = 37287;
const char kTestHtmlPage[] =
    "/media/media_preview/test_site/get_user_media.html";
const base::FilePath::CharType kReferenceVideosDirName[] =
    FILE_PATH_LITERAL("media/media_preview/videos");
const base::FilePath::CharType kReferenceVideoNameYellowImage[] =
    FILE_PATH_LITERAL("yellow_image");
const base::FilePath::CharType kY4mFileExtension[] = FILE_PATH_LITERAL("y4m");

constexpr char kRequestCamera[] = R"(
    new Promise(async resolve => {
      var constraints = { video: true };
      window.focus();
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);
        resolve('granted');
      } catch(error) {
        resolve('denied')
      }
    })
    )";

class PermissionPromptPreviewBrowserTest : public UiBrowserTest {
 public:
  PermissionPromptPreviewBrowserTest() = default;

  PermissionPromptPreviewBrowserTest(
      const PermissionPromptPreviewBrowserTest&) = delete;
  PermissionPromptPreviewBrowserTest& operator=(
      const PermissionPromptPreviewBrowserTest&) = delete;

  base::FilePath GetReferenceFilesDir() {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    return test_data_dir.Append(kReferenceVideosDirName);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start(kFixedPort));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    base::FilePath input_video = GetReferenceFilesDir()
                                     .Append(kReferenceVideoNameYellowImage)
                                     .AddExtension(kY4mFileExtension);
    command_line->AppendSwitchPath(switches::kUseFileForFakeVideoCapture,
                                   input_video);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kCameraMicPreview);
    InProcessBrowserTest::SetUp();
  }

  void ShowUi(const std::string& name) override {
    GURL url = embedded_test_server()->GetURL(kTestHtmlPage);
    TabStripModel* tab_strip = browser()->tab_strip_model();
    content::WebContents* embedder_contents = tab_strip->GetActiveWebContents();
    ASSERT_TRUE(embedder_contents);
    content::RenderFrameHost* render_frame_host =
        ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                                  url, 1);
    // Request Notification permission
    EXPECT_TRUE(content::ExecJs(
        render_frame_host, kRequestCamera,
        content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "PermissionPromptBubbleBaseView");
    views::Widget* widget = waiter.WaitIfNeededAndGet();
    EXPECT_NE(widget, nullptr);
  }

  bool VerifyUi() override {
    views::Widget* permission_prompt =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser())
            ->GetPromptWindow();
    EXPECT_TRUE(permission_prompt->IsVisible());
    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(permission_prompt, test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    ui_test_utils::WaitForBrowserToClose();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Shows a permission prompt with a fake camera stream and verifies the UI.
IN_PROC_BROWSER_TEST_F(PermissionPromptPreviewBrowserTest, InvokeUi_camera) {
  ShowAndVerifyUi();
}
