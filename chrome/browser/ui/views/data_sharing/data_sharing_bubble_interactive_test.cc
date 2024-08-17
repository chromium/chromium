// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {
class DataSharingBubbleInteractiveUiTest : public InteractiveBrowserTest {
 public:
  DataSharingBubbleInteractiveUiTest() = default;
  ~DataSharingBubbleInteractiveUiTest() override = default;
  DataSharingBubbleInteractiveUiTest(
      const DataSharingBubbleInteractiveUiTest&) = delete;
  void operator=(const DataSharingBubbleInteractiveUiTest&) = delete;

  auto ShowBubble() {
    return Do(base::BindLambdaForTesting([&]() {
      base::RunLoop run_loop;
      views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
      observer.set_initialized_callback(
          base::BindLambdaForTesting([&](views::Widget* w) {
            if (w->GetName() == "DataSharingBubbleDialogView") {
              run_loop.Quit();
            }
          }));
      auto* controller =
          DataSharingBubbleController::GetOrCreateForBrowser(browser());
      controller->Show();
      run_loop.Run();
    }));
  }

  auto CloseBubble() {
    return Do(base::BindLambdaForTesting([&]() {
      base::RunLoop run_loop;
      views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
      observer.set_closing_callback(
          base::BindLambdaForTesting([&](views::Widget* w) {
            if (w->GetName() == "DataSharingBubbleDialogView") {
              run_loop.Quit();
            }
          }));
      auto* controller = DataSharingBubbleController::FromBrowser(browser());
      controller->Close();
      run_loop.Run();
    }));
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      data_sharing::features::kDataSharingFeature};
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_BubbleCanShowAndClose DISABLED_BubbleCanShowAndClose
#else
#define MAYBE_BubbleCanShowAndClose BubbleCanShowAndClose
#endif
IN_PROC_BROWSER_TEST_F(DataSharingBubbleInteractiveUiTest,
                       MAYBE_BubbleCanShowAndClose) {
  RunTestSequence(EnsureNotPresent(kDataSharingBubbleElementId), ShowBubble(),
                  WaitForShow(kDataSharingBubbleElementId), CloseBubble(),
                  WaitForHide(kDataSharingBubbleElementId));
}
}  // namespace
