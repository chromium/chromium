// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/web_dialogs/test/test_web_dialog_delegate.h"
#include "url/gurl.h"

namespace {

class TestConstrainedWebDialogDelegate : public ConstrainedWebDialogDelegate {
 public:
  TestConstrainedWebDialogDelegate() {
    web_dialog_delegate_ = std::make_unique<ui::WebDialogDelegate>();
    web_dialog_delegate_->set_delete_on_close(false);
  }

  // ConstrainedWebDialogDelegate::GetWebDialogDelegate w/ covariant returns
  const ui::WebDialogDelegate* GetWebDialogDelegate() const override {
    return web_dialog_delegate_.get();
  }
  ui::WebDialogDelegate* GetWebDialogDelegate() override {
    return web_dialog_delegate_.get();
  }

  // ConstrainedWebDialogDelegate:

  void OnDialogCloseFromWebUI() override {}

  std::unique_ptr<content::WebContents> ReleaseWebContents() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  content::WebContents* GetWebContents() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  gfx::NativeWindow GetNativeDialog() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  gfx::Size GetConstrainedWebDialogMinimumSize() const override {
    NOTREACHED_IN_MIGRATION();
    return gfx::Size();
  }

  gfx::Size GetConstrainedWebDialogMaximumSize() const override {
    NOTREACHED_IN_MIGRATION();
    return gfx::Size();
  }

  gfx::Size GetConstrainedWebDialogPreferredSize() const override {
    NOTREACHED_IN_MIGRATION();
    return gfx::Size();
  }

 private:
  std::unique_ptr<ui::WebDialogDelegate> web_dialog_delegate_;
};

}  // namespace

class ConstrainedWebDialogUITest : public ::testing::Test {
 public:
  ConstrainedWebDialogUITest() = default;
  ~ConstrainedWebDialogUITest() override = default;

 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());

    dialog_delegate_ = std::make_unique<TestConstrainedWebDialogDelegate>();
    dialog_ = std::make_unique<ConstrainedWebDialogUI>(web_ui_.get());
    dialog_->WebUIRenderFrameCreated(web_contents_->GetPrimaryMainFrame());

    ConstrainedWebDialogUI::SetConstrainedDelegate(web_contents_.get(),
                                                   dialog_delegate_.get());
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  TestConstrainedWebDialogDelegate* dialog_delegate() {
    return dialog_delegate_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;

  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<ConstrainedWebDialogUI> dialog_;
  std::unique_ptr<TestConstrainedWebDialogDelegate> dialog_delegate_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// Demonstrates that empty args list won't cause a crash: crbug.com/1262467.
TEST_F(ConstrainedWebDialogUITest, DialogCloseWithEmptyArgs) {
  base::RunLoop run_loop;
  dialog_delegate()->GetWebDialogDelegate()->RegisterOnDialogClosedCallback(
      base::BindLambdaForTesting([&](const std::string& json_retval) {
        ASSERT_EQ(json_retval, "");
        run_loop.Quit();
      }));
  base::Value::List args;
  web_ui()->HandleReceivedMessage("dialogClose", args);
  run_loop.Run();
}

TEST_F(ConstrainedWebDialogUITest, DialogCloseWithJsonInArgs) {
  const std::string kJsonRetval = "[42]";
  std::string json_retval;
  base::RunLoop run_loop;
  dialog_delegate()->GetWebDialogDelegate()->RegisterOnDialogClosedCallback(
      base::BindLambdaForTesting([&](const std::string& cb_json_retval) {
        json_retval = cb_json_retval;
        run_loop.Quit();
      }));
  base::Value::List args;
  args.Append(kJsonRetval);
  web_ui()->HandleReceivedMessage("dialogClose", args);
  run_loop.Run();
  ASSERT_EQ(json_retval, kJsonRetval);
}
