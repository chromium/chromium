// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error/global_error_service.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace {

// An error that has a bubble view.
class BubbleViewError final : public GlobalErrorWithStandardBubble {
 public:
  BubbleViewError() : bubble_view_close_count_(0) { }

  BubbleViewError(const BubbleViewError&) = delete;
  BubbleViewError& operator=(const BubbleViewError&) = delete;

  int bubble_view_close_count() { return bubble_view_close_count_; }

  bool HasMenuItem() override { return false; }
  int MenuItemCommandID() override {
    ADD_FAILURE();
    return 0;
  }
  std::u16string MenuItemLabel() override {
    ADD_FAILURE();
    return std::u16string();
  }
  void ExecuteMenuItem(Browser* browser) override { ADD_FAILURE(); }

  bool HasBubbleView() override { return true; }
  std::u16string GetBubbleViewTitle() override { return std::u16string(); }
  std::vector<std::u16string> GetBubbleViewMessages() override {
    return std::vector<std::u16string>();
  }
  std::u16string GetBubbleViewAcceptButtonLabel() override { return u"OK"; }
  std::u16string GetBubbleViewCancelButtonLabel() override { return u"Cancel"; }
  void OnBubbleViewDidClose(Browser* browser) override {
    EXPECT_TRUE(browser);
    ++bubble_view_close_count_;
  }
  void BubbleViewAcceptButtonPressed(Browser* browser) override {}
  void BubbleViewCancelButtonPressed(Browser* browser) override {}
  base::WeakPtr<GlobalErrorWithStandardBubble> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  int bubble_view_close_count_;
  base::WeakPtrFactory<BubbleViewError> weak_ptr_factory_{this};
};

} // namespace

class GlobalErrorServiceBrowserTest : public InProcessBrowserTest {
};

// Test that showing a error with a bubble view works.
IN_PROC_BROWSER_TEST_F(GlobalErrorServiceBrowserTest, ShowBubbleView) {
  // This will be deleted by the GlobalErrorService.
  BubbleViewError* error = new BubbleViewError;

  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(browser()->profile());
  service->AddGlobalError(base::WrapUnique(error));

  EXPECT_EQ(error, service->GetFirstGlobalErrorWithBubbleView());
  EXPECT_FALSE(error->HasShownBubbleView());
  EXPECT_EQ(0, error->bubble_view_close_count());

  // Creating a second browser window should show the bubble view.
  CreateBrowser(browser()->profile());
  EXPECT_EQ(nullptr, service->GetFirstGlobalErrorWithBubbleView());
  EXPECT_TRUE(error->HasShownBubbleView());
  EXPECT_EQ(0, error->bubble_view_close_count());
}

// Test that GlobalErrorBubbleViewBase::CloseBubbleView correctly closes the
// bubble view.
IN_PROC_BROWSER_TEST_F(GlobalErrorServiceBrowserTest, CloseBubbleView) {
  // This will be deleted by the GlobalErrorService.
  BubbleViewError* error = new BubbleViewError;

  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(browser()->profile());
  service->AddGlobalError(base::WrapUnique(error));

  EXPECT_EQ(error, service->GetFirstGlobalErrorWithBubbleView());
  EXPECT_FALSE(error->HasShownBubbleView());
  EXPECT_EQ(0, error->bubble_view_close_count());

  // Creating a second browser window should show the bubble view.
  CreateBrowser(browser()->profile());
  EXPECT_EQ(nullptr, service->GetFirstGlobalErrorWithBubbleView());
  EXPECT_TRUE(error->HasShownBubbleView());
  EXPECT_EQ(0, error->bubble_view_close_count());

  // Explicitly close the bubble view.
  EXPECT_TRUE(error->GetBubbleView());
  error->GetBubbleView()->CloseBubbleView();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, error->bubble_view_close_count());
}

// Test that bubble is silently dismissed if it is showing when the GlobalError
// instance is removed from the profile.
//
// This uses the deprecated "unowned" API to the GlobalErrorService to maintain
// coverage. When those calls are eventually removed (http://crbug.com/673578)
// these uses should be switched to the non-deprecated API.
// TODO(crbug.com/41485585): Flaky on asan lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_BubbleViewDismissedOnRemove DISABLED_BubbleViewDismissedOnRemove
#else
#define MAYBE_BubbleViewDismissedOnRemove BubbleViewDismissedOnRemove
#endif
IN_PROC_BROWSER_TEST_F(GlobalErrorServiceBrowserTest,
                       MAYBE_BubbleViewDismissedOnRemove) {
  std::unique_ptr<BubbleViewError> error(new BubbleViewError);

  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(browser()->profile());
  service->AddUnownedGlobalError(error.get());

  EXPECT_EQ(error.get(), service->GetFirstGlobalErrorWithBubbleView());
  error->ShowBubbleView(browser());
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(error->HasShownBubbleView());
  EXPECT_EQ(0, error->bubble_view_close_count());

  // Removing |error| from profile should dismiss the bubble view without
  // calling |error->BubbleViewDidClose|.
  service->RemoveUnownedGlobalError(error.get());
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(1, error->bubble_view_close_count());
}
