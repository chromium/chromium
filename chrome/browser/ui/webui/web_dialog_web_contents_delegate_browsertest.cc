// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

using content::BrowserContext;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using ui::WebDialogWebContentsDelegate;

namespace {

class TestWebContentsDelegate : public WebDialogWebContentsDelegate {
 public:
  explicit TestWebContentsDelegate(content::BrowserContext* context)
      : WebDialogWebContentsDelegate(
            context,
            std::make_unique<ChromeWebContentsHandler>()) {}

  TestWebContentsDelegate(const TestWebContentsDelegate&) = delete;
  TestWebContentsDelegate& operator=(const TestWebContentsDelegate&) = delete;

  ~TestWebContentsDelegate() override = default;
};

class TestFileSelectListener : public content::FileSelectListener {
 public:
  explicit TestFileSelectListener(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  bool canceled() const { return canceled_; }

 private:
  ~TestFileSelectListener() override = default;

  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode) override {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void FileSelectionCanceled() override {
    canceled_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  bool canceled_ = false;
  base::OnceClosure quit_closure_;
};

class WebDialogWebContentsDelegateTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    test_web_contents_delegate_ =
        std::make_unique<TestWebContentsDelegate>(browser()->GetProfile());
  }

  void TearDownOnMainThread() override {
    test_web_contents_delegate_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<TestWebContentsDelegate> test_web_contents_delegate_;
};

IN_PROC_BROWSER_TEST_F(WebDialogWebContentsDelegateTest, DoNothingMethodsTest) {
  // None of the following calls should add more tabs or browsers.
  history::HistoryAddPageArgs should_add_args(
      GURL(), base::Time::Now(), 0, 0, std::nullopt, GURL(),
      history::RedirectList(), ui::PAGE_TRANSITION_TYPED, false,
      history::SOURCE_SYNCED, history::VisitResponseCodeCategory::kNot404,
      false, true);
  test_web_contents_delegate_->NavigationStateChanged(
      nullptr, content::InvalidateTypes(0));
  test_web_contents_delegate_->ActivateContents(nullptr);
  test_web_contents_delegate_->LoadingStateChanged(nullptr, true);
  test_web_contents_delegate_->CloseContents(nullptr);
  test_web_contents_delegate_->UpdateTargetURL(nullptr, GURL());
  test_web_contents_delegate_->SetContentsBounds(nullptr, gfx::Rect());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1U, GlobalBrowserCollection::GetInstance()->GetSize());
}

IN_PROC_BROWSER_TEST_F(WebDialogWebContentsDelegateTest, OpenURLFromTabTest) {
  test_web_contents_delegate_->OpenURLFromTab(
      nullptr,
      OpenURLParams::CreateBrowserInitiated(
          GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK),
      /*navigation_handle_callback=*/{});
  // This should create a new foreground tab in the existing browser.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1U, GlobalBrowserCollection::GetInstance()->GetSize());
}

IN_PROC_BROWSER_TEST_F(WebDialogWebContentsDelegateTest,
                       AddNewContentsForegroundTabTest) {
  std::unique_ptr<WebContents> contents =
      WebContents::Create(WebContents::CreateParams(browser()->GetProfile()));
  test_web_contents_delegate_->AddNewContents(
      nullptr, std::move(contents), GURL(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, blink::mojom::WindowFeatures(),
      false, nullptr);
  // This should create a new foreground tab in the existing browser.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1U, GlobalBrowserCollection::GetInstance()->GetSize());
}

IN_PROC_BROWSER_TEST_F(WebDialogWebContentsDelegateTest, DetachTest) {
  EXPECT_EQ(static_cast<content::BrowserContext*>(browser()->GetProfile()),
            test_web_contents_delegate_->browser_context());
  test_web_contents_delegate_->Detach();
  EXPECT_EQ(nullptr, test_web_contents_delegate_->browser_context());
  // None of the following calls should add more tabs or browsers.
  GURL url(url::kAboutBlankURL);
  test_web_contents_delegate_->OpenURLFromTab(
      nullptr,
      OpenURLParams::CreateBrowserInitiated(
          url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK),
      /*navigation_handle_callback=*/{});
  test_web_contents_delegate_->AddNewContents(
      nullptr, nullptr, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      blink::mojom::WindowFeatures(), false, nullptr);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1U, GlobalBrowserCollection::GetInstance()->GetSize());
}

IN_PROC_BROWSER_TEST_F(WebDialogWebContentsDelegateTest,
                       RunFileChooserCanceledForUnsuitableProfileTest) {
  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();
  base::ScopedClosureRunner factory_reset_runner(
      base::BindOnce(&ui::SelectFileDialog::SetFactory, nullptr));

  // Use a non-primary OTR profile, which does not allow browser windows and
  // therefore is also unsuitable for file selection dialogs.
  Profile* otr_profile = browser()->GetProfile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  ASSERT_FALSE(otr_profile->AllowsBrowserWindows());

  std::unique_ptr<WebContents> web_contents =
      WebContents::Create(WebContents::CreateParams(otr_profile));
  TestWebContentsDelegate delegate(otr_profile);

  base::RunLoop run_loop;
  factory->SetOpenCallback(run_loop.QuitClosure());
  auto listener =
      base::MakeRefCounted<TestFileSelectListener>(run_loop.QuitClosure());
  delegate.RunFileChooser(web_contents->GetPrimaryMainFrame(), listener,
                          blink::mojom::FileChooserParams());
  run_loop.Run();

  EXPECT_TRUE(listener->canceled());
  EXPECT_EQ(nullptr, factory->GetLastDialog());
}

}  // namespace
