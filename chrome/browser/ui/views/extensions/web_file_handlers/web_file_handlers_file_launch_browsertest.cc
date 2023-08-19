// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/file_handlers/web_file_handlers_permission_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/extensions/web_file_handlers/web_file_handlers_file_launch_dialog.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/filename_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

// Write file to disk.
base::FilePath WriteFile(const base::FilePath& directory,
                         const base::StringPiece name,
                         const base::StringPiece content) {
  const base::FilePath path = directory.Append(base::StringPiece(name));
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::WriteFile(path, content);
  return path;
}

}  // namespace

class WebFileHandlersFileLaunchBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  WebFileHandlersFileLaunchBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionWebFileHandlers);
  }

 protected:
  static constexpr char const kManifest[] = R"({
    "name": "Test",
    "version": "0.0.1",
    "manifest_version": 3,
    "file_handlers": [
      {
        "name": "Comma separated values",
        "action": "/open-csv.html",
        "accept": {"text/csv": [".csv"]}
      }
    ]
  })";

  const extensions::Extension* WriteDirForFileHandlingExtension() {
    // Load extension.
    extension_dir_.WriteManifest(kManifest);
    extension_dir_.WriteFile("open-csv.js", R"(
      chrome.test.assertTrue('launchQueue' in window);
      launchQueue.setConsumer((launchParams) => {
        chrome.test.assertEq(1, launchParams.files.length);
        chrome.test.assertEq("a.csv", launchParams.files[0].name);
        chrome.test.assertEq("file", launchParams.files[0].kind);
        chrome.test.succeed();
      });
    )");
    extension_dir_.WriteFile("open-csv.html",
                             R"(<script src="/open-csv.js"></script>"
                              "<body>Test</body>)");
    const extensions::Extension* extension =
        LoadExtension(extension_dir_.UnpackedPath());

    return extension;
  }

  // Verify that the launch result matches expectations.
  void VerifyLaunchResult(base::RepeatingClosure quit_closure,
                          apps::LaunchResult::State expected,
                          apps::LaunchResult&& launch_result) {
    ASSERT_EQ(expected, launch_result.state);
    std::move(quit_closure).Run();
  }

  // Launch the extension and accept the dialog.
  void LaunchExtensionAndAcceptDialog(const extensions::Extension& extension) {
    std::unique_ptr<apps::Intent> intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebFileHandlersFileLaunchDialogView");
    // Prepare to launch with intent.
    Profile* const profile = browser()->profile();
    const int32_t event_flags =
        apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                            /*prefer_container=*/true);

    // launch.
    base::RunLoop run_loop;
    apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
        extension.id(), event_flags, std::move(intent),
        apps::LaunchSource::kFromFileManager, nullptr,
        base::BindOnce(
            &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
            base::Unretained(this), run_loop.QuitClosure(),
            apps::LaunchResult::State::SUCCESS));

    extensions::ResultCatcher catcher;
    auto* widget = waiter.WaitIfNeededAndGet();
    widget->widget_delegate()->AsDialogDelegate()->AcceptDialog();
    ASSERT_TRUE(catcher.GetNextResult());
    run_loop.Run();
  }

  // Launch the extension and cancel the dialog.
  void LaunchExtensionAndCancelDialog(const extensions::Extension& extension) {
    std::unique_ptr<apps::Intent> intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebFileHandlersFileLaunchDialogView");

    // Launch app with intent.
    Profile* const profile = browser()->profile();
    const int32_t event_flags =
        apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                            /*prefer_container=*/true);

    // Launch and verify result.
    base::RunLoop run_loop;
    apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
        extension.id(), event_flags, std::move(intent),
        apps::LaunchSource::kFromFileManager, nullptr,
        base::BindOnce(
            &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
            base::Unretained(this), run_loop.QuitClosure(),
            apps::LaunchResult::State::FAILED));

    auto* widget = waiter.WaitIfNeededAndGet();
    widget->widget_delegate()->AsDialogDelegate()->CancelDialog();
    run_loop.Run();
  }

  // Launch the extension and cancel the dialog.
  void LaunchExtensionAndCloseDialog(const extensions::Extension& extension) {
    std::unique_ptr<apps::Intent> intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebFileHandlersFileLaunchDialogView");

    // Launch app with intent.
    Profile* const profile = browser()->profile();
    const int32_t event_flags =
        apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                            /*prefer_container=*/true);

    // Launch and verify result.
    base::RunLoop run_loop;
    apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
        extension.id(), event_flags, std::move(intent),
        apps::LaunchSource::kFromFileManager, nullptr,
        base::BindOnce(
            &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
            base::Unretained(this), run_loop.QuitClosure(),
            apps::LaunchResult::State::FAILED));

    auto* widget = waiter.WaitIfNeededAndGet();

    widget->Close();
    run_loop.Run();
  }

  // Launch the extension and cancel the dialog.
  void LaunchExtensionAndRememberAcceptDialog(
      const extensions::Extension& extension) {
    std::unique_ptr<apps::Intent> intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebFileHandlersFileLaunchDialogView");

    // Launch app with intent.
    Profile* const profile = browser()->profile();
    const int32_t event_flags =
        apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                            /*prefer_container=*/true);

    // Set the checkbox to checked.
    extensions::file_handlers::SetDefaultRememberSelectionForTesting(true);

    // Run the first time.
    {
      // Launch and verify result.
      base::RunLoop run_loop;
      apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
          extension.id(), event_flags, std::move(intent),
          apps::LaunchSource::kFromFileManager, nullptr,
          base::BindOnce(
              &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
              base::Unretained(this), run_loop.QuitClosure(),
              apps::LaunchResult::State::SUCCESS));

      // Open the window.
      extensions::ResultCatcher catcher;
      auto* widget = waiter.WaitIfNeededAndGet();
      widget->widget_delegate()->AsDialogDelegate()->AcceptDialog();
      ASSERT_TRUE(catcher.GetNextResult());
      run_loop.Run();
    }

    // Reopen the window, bypassing the dialog.
    {
      // Second intent.
      std::unique_ptr<apps::Intent> second_intent =
          SetupLaunchAndGetIntent(extension);

      extensions::ResultCatcher second_catcher;

      // Launch and verify result.
      base::RunLoop run_loop;
      apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
          extension.id(), event_flags, std::move(second_intent),
          apps::LaunchSource::kFromFileManager, nullptr,
          base::BindOnce(
              &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
              base::Unretained(this), run_loop.QuitClosure(),
              apps::LaunchResult::State::SUCCESS));

      ASSERT_TRUE(second_catcher.GetNextResult());
      run_loop.Run();
    }
  }

  // Launch the extension and cancel the dialog.
  void LaunchExtensionAndRememberCancelDialog(
      const extensions::Extension& extension) {
    std::unique_ptr<apps::Intent> intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebFileHandlersFileLaunchDialogView");

    // Launch app with intent.
    Profile* const profile = browser()->profile();
    const int32_t event_flags =
        apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                            /*prefer_container=*/true);

    // Set the checkbox to checked.
    extensions::file_handlers::SetDefaultRememberSelectionForTesting(true);

    // Launch for the first time.
    {
      // Launch and verify result.
      base::RunLoop run_loop;
      apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
          extension.id(), event_flags, std::move(intent),
          apps::LaunchSource::kFromFileManager, nullptr,
          base::BindOnce(
              &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
              base::Unretained(this), run_loop.QuitClosure(),
              apps::LaunchResult::State::FAILED));

      auto* widget = waiter.WaitIfNeededAndGet();

      // "Don't Open" the window.
      widget->widget_delegate()->AsDialogDelegate()->CancelDialog();
      run_loop.Run();
    }

    // Run a second time.
    {
      // Second intent.
      std::unique_ptr<apps::Intent> second_intent =
          SetupLaunchAndGetIntent(extension);

      // Reopen the window, bypassing the dialog.
      extensions::ResultCatcher second_catcher;

      // Launch and verify result.
      base::RunLoop run_loop;
      apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
          extension.id(), event_flags, std::move(second_intent),
          apps::LaunchSource::kFromFileManager, nullptr,
          base::BindOnce(
              &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
              base::Unretained(this), run_loop.QuitClosure(),
              apps::LaunchResult::State::FAILED));
      run_loop.Run();
    }
  }

  // Launch the extension and cancel the dialog.
  void LaunchExtensionAndRememberCloseDialog(
      const extensions::Extension& extension) {
    std::unique_ptr<apps::Intent> intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebFileHandlersFileLaunchDialogView");

    // Launch app with intent.
    Profile* const profile = browser()->profile();
    const int32_t event_flags =
        apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                            /*prefer_container=*/true);

    // Set the checkbox to checked.
    extensions::file_handlers::SetDefaultRememberSelectionForTesting(true);

    // Launch for the first time.
    {
      // Launch and verify result.
      base::RunLoop run_loop;
      apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
          extension.id(), event_flags, std::move(intent),
          apps::LaunchSource::kFromFileManager, nullptr,
          base::BindOnce(
              &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
              base::Unretained(this), run_loop.QuitClosure(),
              apps::LaunchResult::State::FAILED));

      // Don't open the file.
      auto* widget = waiter.WaitIfNeededAndGet();
      widget->Close();
      run_loop.Run();
    }

    // Launch for the second time.
    {
      // Second intent.
      std::unique_ptr<apps::Intent> second_intent =
          SetupLaunchAndGetIntent(extension);

      // Reopen the window, bypassing the dialog.
      extensions::ResultCatcher second_catcher;

      views::NamedWidgetShownWaiter second_waiter(
          views::test::AnyWidgetTestPasskey{},
          "WebFileHandlersFileLaunchDialogView");

      // Launch and verify result.
      base::RunLoop run_loop;
      apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
          extension.id(), event_flags, std::move(second_intent),
          apps::LaunchSource::kFromFileManager, nullptr,
          base::BindOnce(
              &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
              base::Unretained(this), run_loop.QuitClosure(),
              apps::LaunchResult::State::FAILED));

      // A widget should be available, indicating that close isn't remembered.
      auto* second_widget = second_waiter.WaitIfNeededAndGet();
      ASSERT_TRUE(second_widget);
      second_widget->Close();
      run_loop.Run();
    }
  }

 private:
  apps::IntentPtr SetupLaunchAndGetIntent(
      const extensions::Extension& extension) {
    auto* file_handlers =
        extensions::WebFileHandlers::GetFileHandlers(extension);
    EXPECT_EQ(1u, file_handlers->size());

    // Create file(s).
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ScopedTempDir scoped_temp_dir;
    EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
    auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionView);
    intent->mime_type = "text/csv";
    intent->activity_name = "open-csv.html";
    const base::FilePath file_path =
        WriteFile(scoped_temp_dir.GetPath(), "a.csv", "1,2,3");

    // Add file(s) to intent.
    int64_t file_size = 0;
    base::GetFileSize(file_path, &file_size);

    // Create a virtual file in the file system, as required for AppService.
    scoped_refptr<storage::FileSystemContext> file_system_context =
        storage::CreateFileSystemContextForTesting(
            /*quota_manager_proxy=*/nullptr, base::FilePath());
    auto file_system_url = file_system_context->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting("chrome://file-manager"),
        storage::kFileSystemTypeTest, file_path);

    // Update the intent with the file.
    auto file = std::make_unique<apps::IntentFile>(file_system_url.ToGURL());
    file->file_name = base::SafeBaseName::Create("a.csv");
    file->file_size = file_size;
    file->mime_type = "text/csv";
    intent->files.push_back(std::move(file));
    return intent;
  }

  extensions::TestExtensionDir extension_dir_;

  base::test::ScopedFeatureList feature_list_;
  // TODO(crbug.com/1448893): Remove channel overrides when available in stable.
  extensions::ScopedCurrentChannel current_channel_{
      version_info::Channel::BETA};
};

// Web File Handlers may require acknowledgement before opening any of the
// manifest-declared file types for the first time. One button opens the file
// and the other does not. The selection can be remembered through the use of a
// checkbox. Open, don't open, and escape from the permission dialog. Then,
// remember opening a file, followed by opening again while bypassing the
// dialog. `Remember my choice` is stored as a boolean at the extension level,
// not on a per file type basis.
IN_PROC_BROWSER_TEST_F(WebFileHandlersFileLaunchBrowserTest,
                       WebFileHandlersPermissionHandler) {
  // Install and get extension.
  auto* extension = WriteDirForFileHandlingExtension();
  ASSERT_TRUE(extension);

  // Test opening a file after being presented with the permission handler UI.
  LaunchExtensionAndAcceptDialog(*extension);
  LaunchExtensionAndCancelDialog(*extension);
  LaunchExtensionAndCloseDialog(*extension);
  LaunchExtensionAndRememberAcceptDialog(*extension);
}

// Clicking `Don't Open` should be remembered for all associated file types.
// That's because it's stored as a boolean at the extension level, rather than
// for each file type. `Cancel` and `Close` both dismiss the UI without opening
// the file. The difference is that `Cancel` will `Remember my choice`, but
// `Close` will not.
IN_PROC_BROWSER_TEST_F(WebFileHandlersFileLaunchBrowserTest,
                       WebFileHandlersPermissionHandlerRememberCancel) {
  // Install and get extension.
  auto* extension = WriteDirForFileHandlingExtension();
  ASSERT_TRUE(extension);

  // Clicking "Don't Open" should remember that choice for the file extension.
  LaunchExtensionAndRememberCancelDialog(*extension);
}

// Closing the dialog does not remember that choice, even if selected. An
// example of closing would be pressing escape or clicking an x, if present.
IN_PROC_BROWSER_TEST_F(WebFileHandlersFileLaunchBrowserTest,
                       WebFileHandlersPermissionHandlerRememberClose) {
  // Install and get extension.
  auto* extension = WriteDirForFileHandlingExtension();
  ASSERT_TRUE(extension);

  // e.g. pressing escape to close the dialog shouldn't remember that choice.
  LaunchExtensionAndRememberCloseDialog(*extension);
}
