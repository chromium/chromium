// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_menu_model_adapter.h"
#include "ash/shell.h"
#include "base/path_service.h"
#include "chrome/browser/ui/ash/clipboard_history_test_util.h"
#include "chrome/browser/ui/ash/clipboard_image_model_request.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/events/test/event_generator.h"

namespace {
using ImageModelRequestTestParams = ClipboardImageModelRequest::TestParams;

ash::ClipboardHistoryControllerImpl* GetClipboardHistoryController() {
  return ash::Shell::Get()->clipboard_history_controller();
}

ash::ClipboardHistoryMenuModelAdapter* GetContextMenu() {
  return GetClipboardHistoryController()->context_menu_for_test();
}

const std::list<ash::ClipboardHistoryItem>& GetClipboardItems() {
  return GetClipboardHistoryController()->history()->GetItems();
}

}  // namespace

// TODO(crbug.com/1304484): Make this class inherit from
// `ClipboardHistoryBrowserTest` instead if possible.
class ClipboardHistoryWebContentsInteractiveTest : public InProcessBrowserTest {
 public:
  ClipboardHistoryWebContentsInteractiveTest() = default;
  ~ClipboardHistoryWebContentsInteractiveTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("chrome/test/data/ash/clipboard_history"));
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Verifies that the images rendered from the copied web contents should
// show in the clipboard history menu. Switching the auto resize mode is covered
// in this test case.
// Flaky: crbug/1224777
IN_PROC_BROWSER_TEST_F(ClipboardHistoryWebContentsInteractiveTest,
                       VerifyHTMLRendering) {
  // Load the web page which contains images and text.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/image-and-text.html")));

  // Select one part of the web page. Wait until the selection region updates.
  // Then copy the selected part to clipboard.
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::BoundingBoxUpdateWaiter select_part_one(web_contents);
  ASSERT_TRUE(ExecuteScript(web_contents, "selectPart1();"));
  select_part_one.Wait();

  const auto& item_lists = GetClipboardItems();
  {
    clipboard_history::ScopedClipboardHistoryListUpdateWaiter scoped_waiter;
    ASSERT_TRUE(ExecuteScript(web_contents, "copyToClipboard();"));
  }
  ASSERT_EQ(1u, item_lists.size());

  base::HistogramTester histogram_tester;

  // Show the clipboard history menu through the acclerator. When the clipboard
  // history shows, the process of HTML rendering starts.
  auto event_generator = std::make_unique<ui::test::EventGenerator>(
      ash::Shell::GetPrimaryRootWindow());
  event_generator->PressAndReleaseKey(ui::VKEY_V, ui::EF_COMMAND_DOWN);

  // Render HTML with auto-resize mode enabled. Wait until the rendering
  // finishes.
  ImageModelRequestTestParams test_params(
      /*callback=*/base::NullCallback(),
      /*enforce_auto_resize=*/true);
  {
    clipboard_history::ClipboardImageModelRequestWaiter image_request_waiter(
        &test_params, /*expect_auto_resize=*/true);
    image_request_waiter.Wait();
  }

  // Verify that the rendering ends normally.
  histogram_tester.ExpectUniqueSample(
      "Ash.ClipboardHistory.ImageModelRequest.StopReason",
      static_cast<int>(
          ClipboardImageModelRequest::RequestStopReason::kFulfilled),
      1);

  // Verify that the clipboard history menu shows. Then close the menu.
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  event_generator->PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_FALSE(GetClipboardHistoryController()->IsMenuShowing());

  // Select another part. Wait until the selection region updates. Then copy
  // the selected html code to clipboard.
  content::BoundingBoxUpdateWaiter select_part_two(web_contents);
  ASSERT_TRUE(ExecuteScript(web_contents, "selectPart2();"));
  select_part_two.Wait();

  {
    clipboard_history::ScopedClipboardHistoryListUpdateWaiter scoped_waiter;
    ASSERT_TRUE(ExecuteScript(web_contents, "copyToClipboard();"));
  }
  ASSERT_EQ(2u, item_lists.size());

  // Show the clipboard history menu.
  event_generator->PressAndReleaseKey(ui::VKEY_V, ui::EF_COMMAND_DOWN);

  // Render HTML with auto-resize mode disabled. Wait until the rendering
  // finishes.
  test_params.enforce_auto_resize = false;
  {
    clipboard_history::ClipboardImageModelRequestWaiter image_request_waiter(
        &test_params, /*expect_auto_resize=*/false);
    image_request_waiter.Wait();
  }

  // Verify that the rendering ends normally.
  histogram_tester.ExpectUniqueSample(
      "Ash.ClipboardHistory.ImageModelRequest.StopReason",
      static_cast<int>(
          ClipboardImageModelRequest::RequestStopReason::kFulfilled),
      2);

  // Verify that the clipboard history menu's status.
  EXPECT_TRUE(GetClipboardHistoryController()->IsMenuShowing());
  ASSERT_EQ(2, GetContextMenu()->GetMenuItemsCount());
}
