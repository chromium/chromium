// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_in_progress_dialog_view.h"

#include <optional>

#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/views/widget/any_widget_observer.h"

using DownloadInProgressDialogTest = ChromeViewsTestBase;

// This checks that DownloadInProgressDialogView runs its completion callback
// even if the dialog is closed by the user without selecting an option. It is a
// regression test for https://crbug.com/1064138.
TEST_F(DownloadInProgressDialogTest, CallbackIsRunOnClose) {
  SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());

  auto parent =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  parent->Show();

  std::optional<bool> result;
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey(),
                                       "DownloadInProgressDialogView");
  DownloadInProgressDialogView::Show(
      parent->GetNativeWindow(), 1,
      Browser::DownloadCloseType::kBrowserShutdown,
      base::BindLambdaForTesting([&](bool b) { result = b; }));
  waiter.WaitIfNeededAndGet()->Close();

  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
}
