// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
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
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"
#include "extensions/common/web_file_handler_constants.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/filename_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

// Write file to disk.
base::FilePath WriteFile(const base::FilePath& directory,
                         std::string_view name,
                         std::string_view content) {
  const base::FilePath path = directory.Append(std::string_view(name));
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

  // Verify that the launch result matches expectations.
  void VerifyLaunchResult(base::RepeatingClosure quit_closure,
                          apps::LaunchResult::State expected,
                          apps::LaunchResult&& launch_result) {
    ASSERT_EQ(expected, launch_result.state);
    std::move(quit_closure).Run();
  }

 protected:
  // Install the file path as an extension that's installed by default.
  const extensions::Extension* WriteToDirAndLoadDefaultInstalledExtension(
      const std::string& manifest) {
    WriteToExtensionDir(manifest);
    return InstallExtensionWithSourceAndFlags(
        extension_dir_.UnpackedPath(),
        /*expected_change=*/true,
        extensions::mojom::ManifestLocation::kInternal,
        extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  }

  // Write expected files to the extension directory.
  void WriteToExtensionDir(const std::string& manifest) {
    extension_dir_.WriteManifest(manifest);
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
  }

  // Load an extension with a file handler instance that can be awaited later.
  const extensions::Extension* WriteToDirAndLoadExtension() {
    WriteToExtensionDir(kManifest);
    return LoadExtension(extension_dir_.UnpackedPath());
  }

  const extensions::Extension* LoadAndGetExtension(
      const std::string& manifest) {
    WriteToExtensionDir(manifest);
    return LoadExtension(extension_dir_.UnpackedPath());
  }

  // Load an extension with a few extra files for testing multiple-clients.
  const extensions::Extension* WriteCustomDirForFileHandlingExtension(
      std::string_view manifest,
      const base::flat_map<std::string, std::string>& files) {
    extension_dir_.WriteManifest(manifest);
    for (const auto& [name, content] : files) {
      extension_dir_.WriteFile(name, content);
    }
    const extensions::Extension* extension =
        LoadExtension(extension_dir_.UnpackedPath());

    return extension;
  }

  void LaunchAppWithIntent(apps::IntentPtr intent,
                           const extensions::ExtensionId& extension_id,
                           apps::LaunchCallback callback) {
    const int32_t event_flags =
        apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                            /*prefer_container=*/true);
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->LaunchAppWithIntent(extension_id, event_flags, std::move(intent),
                              apps::LaunchSource::kFromFileManager, nullptr,
                              std::move(callback));
  }

  // Launch the extension from an intent and wait for a result from chrome.test.
  void LaunchExtensionAndCatchResult(const extensions::Extension& extension) {
    apps::IntentPtr intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    // Verify launch.
    extensions::ResultCatcher catcher;
    LaunchAppWithIntent(std::move(intent), extension.id(), base::DoNothing());
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Launch the extension and accept the dialog.
  void LaunchExtensionAndAcceptDialog(const extensions::Extension& extension) {
    apps::IntentPtr intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    // Create waiter to verify if the permission dialog is displayed.
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebFileHandlersFileLaunchDialogView");
    // Launch.
    base::RunLoop run_loop;
    LaunchAppWithIntent(
        std::move(intent), extension.id(),
        base::BindOnce(
            &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
            base::Unretained(this), run_loop.QuitClosure(),
            apps::LaunchResult::State::kSuccess));

    extensions::ResultCatcher catcher;
    auto* widget = waiter.WaitIfNeededAndGet();
    widget->widget_delegate()->AsDialogDelegate()->AcceptDialog();
    ASSERT_TRUE(catcher.GetNextResult());
    run_loop.Run();
  }

  // Launch the extension and cancel the dialog.
  void LaunchExtensionAndCancelDialog(const extensions::Extension& extension) {
    apps::IntentPtr intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebFileHandlersFileLaunchDialogView");
    // Launch and verify result.
    base::RunLoop run_loop;
    LaunchAppWithIntent(
        std::move(intent), extension.id(),
        base::BindOnce(
            &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
            base::Unretained(this), run_loop.QuitClosure(),
            apps::LaunchResult::State::kFailed));

    auto* widget = waiter.WaitIfNeededAndGet();
    widget->widget_delegate()->AsDialogDelegate()->CancelDialog();
    run_loop.Run();
  }

  // Launch the extension and cancel the dialog.
  void LaunchExtensionAndCloseDialog(const extensions::Extension& extension) {
    apps::IntentPtr intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebFileHandlersFileLaunchDialogView");
    // Launch and verify result.
    base::RunLoop run_loop;
    LaunchAppWithIntent(
        std::move(intent), extension.id(),
        base::BindOnce(
            &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
            base::Unretained(this), run_loop.QuitClosure(),
            apps::LaunchResult::State::kFailed));

    auto* widget = waiter.WaitIfNeededAndGet();

    widget->Close();
    run_loop.Run();
  }

  // Launch the extension and cancel the dialog.
  void LaunchExtensionAndRememberAcceptDialog(
      const extensions::Extension& extension) {
    apps::IntentPtr intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebFileHandlersFileLaunchDialogView");
    // Set the checkbox to checked.
    // TODO: handle return value.
    std::ignore =
        extensions::file_handlers::SetDefaultRememberSelectionForTesting(true);

    // Run the first time.
    {
      // Launch and verify result.
      base::RunLoop run_loop;
      LaunchAppWithIntent(
          std::move(intent), extension.id(),
          base::BindOnce(
              &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
              base::Unretained(this), run_loop.QuitClosure(),
              apps::LaunchResult::State::kSuccess));

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
      apps::IntentPtr second_intent = SetupLaunchAndGetIntent(extension);

      extensions::ResultCatcher second_catcher;

      // Launch and verify result.
      base::RunLoop run_loop;
      LaunchAppWithIntent(
          std::move(second_intent), extension.id(),
          base::BindOnce(
              &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
              base::Unretained(this), run_loop.QuitClosure(),
              apps::LaunchResult::State::kSuccess));

      ASSERT_TRUE(second_catcher.GetNextResult());
      run_loop.Run();
    }
  }

  // Launch the extension and cancel the dialog.
  void LaunchExtensionAndRememberCancelDialog(
      const extensions::Extension& extension) {
    apps::IntentPtr intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebFileHandlersFileLaunchDialogView");
    // Set the checkbox to checked.
    // TODO: handle return value.
    std::ignore =
        extensions::file_handlers::SetDefaultRememberSelectionForTesting(true);

    // Launch for the first time.
    {
      // Launch and verify result.
      base::RunLoop run_loop;
      LaunchAppWithIntent(
          std::move(intent), extension.id(),
          base::BindOnce(
              &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
              base::Unretained(this), run_loop.QuitClosure(),
              apps::LaunchResult::State::kFailed));

      auto* widget = waiter.WaitIfNeededAndGet();

      // "Don't Open" the window.
      widget->widget_delegate()->AsDialogDelegate()->CancelDialog();
      run_loop.Run();
    }

    // Run a second time.
    {
      // Second intent.
      apps::IntentPtr second_intent = SetupLaunchAndGetIntent(extension);

      // Reopen the window, bypassing the dialog.
      extensions::ResultCatcher second_catcher;

      // Launch and verify result.
      base::RunLoop run_loop;
      LaunchAppWithIntent(
          std::move(second_intent), extension.id(),
          base::BindOnce(
              &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
              base::Unretained(this), run_loop.QuitClosure(),
              apps::LaunchResult::State::kFailed));
      run_loop.Run();
    }
  }

  // Launch the extension and cancel the dialog.
  void LaunchExtensionAndRememberCloseDialog(
      const extensions::Extension& extension) {
    apps::IntentPtr intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebFileHandlersFileLaunchDialogView");
    // Set the checkbox to checked.
    // TODO: handle return value.
    std::ignore =
        extensions::file_handlers::SetDefaultRememberSelectionForTesting(true);

    // Launch for the first time.
    {
      // Launch and verify result.
      base::RunLoop run_loop;
      LaunchAppWithIntent(
          std::move(intent), extension.id(),
          base::BindOnce(
              &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
              base::Unretained(this), run_loop.QuitClosure(),
              apps::LaunchResult::State::kFailed));

      // Don't open the file.
      auto* widget = waiter.WaitIfNeededAndGet();
      widget->Close();
      run_loop.Run();
    }

    // Launch for the second time.
    {
      // Second intent.
      apps::IntentPtr second_intent = SetupLaunchAndGetIntent(extension);

      // Reopen the window, bypassing the dialog.
      extensions::ResultCatcher second_catcher;

      views::NamedWidgetShownWaiter second_waiter(
          views::test::AnyWidgetTestPasskey{},
          "WebFileHandlersFileLaunchDialogView");

      // Launch and verify result.
      base::RunLoop run_loop;
      LaunchAppWithIntent(
          std::move(second_intent), extension.id(),
          base::BindOnce(
              &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
              base::Unretained(this), run_loop.QuitClosure(),
              apps::LaunchResult::State::kFailed));

      // A widget should be available, indicating that close isn't remembered.
      auto* second_widget = second_waiter.WaitIfNeededAndGet();
      ASSERT_TRUE(second_widget);
      second_widget->Close();
      run_loop.Run();
    }
  }

  apps::IntentPtr CreateFilesToOpen(
      const extensions::Extension& extension,
      const std::string& activity_name,
      const std::string& mime_type,
      const base::flat_map<std::string, std::string>& files) {
    // Create a file.
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ScopedTempDir scoped_temp_dir;
    EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
    auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionView);
    intent->mime_type = mime_type;
    intent->activity_name = activity_name;

    for (const auto& [name, content] : files) {
      const base::FilePath file_path =
          WriteFile(scoped_temp_dir.GetPath(), name, content);

      // Add file(s) to intent.
      int64_t file_size = 0;
      base::GetFileSize(file_path, &file_size);

      // Create a virtual file in the file system, as required by AppService.
      scoped_refptr<storage::FileSystemContext> file_system_context =
          storage::CreateFileSystemContextForTesting(
              /*quota_manager_proxy=*/nullptr, base::FilePath());
      auto file_system_url = file_system_context->CreateCrackedFileSystemURL(
          blink::StorageKey::CreateFromStringForTesting(
              "chrome://file-manager"),
          storage::kFileSystemTypeTest, file_path);

      // Update the intent with the file.
      auto file = std::make_unique<apps::IntentFile>(file_system_url.ToGURL());
      file->file_name = base::SafeBaseName::Create("a.csv");
      file->file_size = file_size;
      file->mime_type = "text/csv";
      intent->files.push_back(std::move(file));
    }

    return intent;
  }

  apps::IntentPtr SetupLaunchAndGetIntent(
      const extensions::Extension& extension) {
    // Verify the number of file handlers in the extension manifest.
    auto* file_handlers =
        extensions::WebFileHandlers::GetFileHandlers(extension);
    EXPECT_EQ(1u, file_handlers->size());

    // Create a file.
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ScopedTempDir scoped_temp_dir;
    EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
    auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionView);
    intent->mime_type = "text/csv";
    intent->activity_name = "/open-csv.html";
    const base::FilePath file_path =
        WriteFile(scoped_temp_dir.GetPath(), "a.csv", "1,2,3");

    // Add file(s) to intent.
    int64_t file_size = 0;
    base::GetFileSize(file_path, &file_size);

    // Create a virtual file in the file system, as required by AppService.
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

 private:
  extensions::TestExtensionDir extension_dir_;

  base::test::ScopedFeatureList feature_list_;

  // Basic manifest for web file handlers.
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
  auto* extension = WriteToDirAndLoadExtension();
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
  auto* extension = WriteToDirAndLoadExtension();
  ASSERT_TRUE(extension);

  // Clicking "Don't Open" should remember that choice for the file extension.
  LaunchExtensionAndRememberCancelDialog(*extension);
}

// Closing the dialog does not remember that choice, even if selected. An
// example of closing would be pressing escape or clicking an x, if present.
IN_PROC_BROWSER_TEST_F(WebFileHandlersFileLaunchBrowserTest,
                       WebFileHandlersPermissionHandlerRememberClose) {
  // Install and get extension.
  auto* extension = WriteToDirAndLoadExtension();
  ASSERT_TRUE(extension);

  // e.g. pressing escape to close the dialog shouldn't remember that choice.
  LaunchExtensionAndRememberCloseDialog(*extension);
}

// Verify that the file opened.
IN_PROC_BROWSER_TEST_F(WebFileHandlersFileLaunchBrowserTest, CallSetConsumer) {
  // Install and get extension.
  auto* extension = WriteToDirAndLoadExtension();
  ASSERT_TRUE(extension);

  // Open a file and remember that selection for automatic reopening.
  LaunchExtensionAndRememberAcceptDialog(*extension);

  // Reopen the file and ensure that it's available in `launchParams`.
  LaunchExtensionAndCatchResult(*extension);
}

// Verify that the Ash component of this ChromeOS feature is on the stable
// channel.
IN_PROC_BROWSER_TEST_F(WebFileHandlersFileLaunchBrowserTest, QuickOffice) {
  const std::string manifest =
      base::StringPrintf(R"({
    "name": "Test",
    "version": "0.0.1",
    "manifest_version": 3,
    "file_handlers": [{
      "name": "Comma separated values",
      "action": "/open-csv.html",
      "accept": {"text/csv": [".csv"]}
    }],
    "key": "%s"
  })",
                         extensions::web_file_handlers::kQuickOfficeKey);

  // Install and get extension.
  auto* extension = LoadAndGetExtension(manifest);
  ASSERT_TRUE(extension);

  // Web File Handlers are supported.
  ASSERT_TRUE(extensions::WebFileHandlers::SupportsWebFileHandlers(*extension));
  ASSERT_TRUE(
      extensions::WebFileHandlers::CanBypassPermissionDialog(*extension));

  // Reopen the file and ensure that it's available in `launchParams`.
  LaunchExtensionAndCatchResult(*extension);
}

// Verify that the Ash component of this ChromeOS feature is on the stable
// channel.
IN_PROC_BROWSER_TEST_F(WebFileHandlersFileLaunchBrowserTest, DefaultInstalled) {
  static constexpr char kManifest[] = R"({
    "name": "Test",
    "version": "0.0.1",
    "manifest_version": 3,
    "file_handlers": [{
      "name": "Comma separated values",
      "action": "/open-csv.html",
      "accept": {"text/csv": [".csv"]}
    }]
  })";

  // Install and get extension.
  auto* extension = WriteToDirAndLoadDefaultInstalledExtension(kManifest);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(extensions::WebFileHandlers::SupportsWebFileHandlers(*extension));
  ASSERT_TRUE(
      extensions::WebFileHandlers::CanBypassPermissionDialog(*extension));
}

// Verify `{"launch_type": "multiple-clients"}`.
IN_PROC_BROWSER_TEST_F(WebFileHandlersFileLaunchBrowserTest,
                       LaunchTypeMultipleClients) {
  // Create a manifest that includes `{"launch_type": "multiple-clients"}`.
  static constexpr char kManifest[] = R"({
    "name": "Test",
    "version": "0.0.1",
    "manifest_version": 3,
    "file_handlers": [
      {
        "name": "single-client",
        "action": "/open-csv.html",
        "accept": {"text/csv": [".csv"]}
      },
      {
        "name": "multiple-clients",
        "action": "/open-txt.html",
        "accept": {"text/plain": [".txt"]},
        "launch_type": "multiple-clients"
      }
    ]
  })";

  static constexpr char kScript[] = R"(
        chrome.test.assertTrue('launchQueue' in window);
        launchQueue.setConsumer((launchParams) => {
          chrome.test.assertEq(%d, launchParams.files.length);
          chrome.test.assertTrue(
              launchParams.files.every(file => file.kind === "file"));
          chrome.test.succeed();
        });
      )";

  static constexpr char kScriptTag[] =
      R"(<script src="%s"></script><body>Test</body>)";

  // Create a map of one filename for each file content.
  base::flat_map<std::string, std::string> files = {
      {"open-csv.js", base::StringPrintf(kScript, /*expected=*/2)},
      {"open-csv.html", base::StringPrintf(kScriptTag, "/open-csv.js")},
      {"open-txt.js", base::StringPrintf(kScript, /*expected=*/1)},
      {"open-txt.html", base::StringPrintf(kScriptTag, "/open-txt.js")},
  };

  // Load extension.
  auto* extension = WriteCustomDirForFileHandlingExtension(kManifest, files);
  ASSERT_TRUE(extension);

  auto VerifyLaunch =
      [&](const std::string& activity_name, const std::string& mime_type,
          const base::flat_map<std::string, std::string>& files_to_open) {
        auto intent = CreateFilesToOpen(*extension, activity_name, mime_type,
                                        files_to_open);
        base::RunLoop run_loop;
        // Create waiter to verify if the permission dialog is displayed.
        views::NamedWidgetShownWaiter waiter(
            views::test::AnyWidgetTestPasskey{},
            "WebFileHandlersFileLaunchDialogView");

        // Open multiple files in single client mode and remember the selection.
        LaunchAppWithIntent(
            std::move(intent), extension->id(),
            base::BindOnce(
                &WebFileHandlersFileLaunchBrowserTest::VerifyLaunchResult,
                base::Unretained(this), run_loop.QuitClosure(),
                apps::LaunchResult::State::kSuccess));

        auto* widget = waiter.WaitIfNeededAndGet();
        extensions::ResultCatcher catcher;
        widget->widget_delegate()->AsDialogDelegate()->AcceptDialog();
        ASSERT_TRUE(catcher.GetNextResult());
        run_loop.Run();
      };

  struct {
    std::string test_name;
    std::string activity_name;
    std::string mime_type;
    base::flat_map<std::string, std::string> files_to_open;
  } test_cases[] = {
      // clang-format off

    // single-client (default if unset) opens all files in the same tab.
    {
      "Verify single-client",
      "/open-csv.html",
      "text/csv",
      {
        {"a.csv", "a csv"},
        {"b.csv", "b csv"}
      }
    },

    // multiple-clients opens a new tab for each file.
    {
      "Verify multiple-clients",
      "/open-txt.html",
      "text/plain",
      {
        {"a.txt", "a txt"},
        {"b.txt", "b txt"}
      }
    }

      // clang-format on
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.test_name);
    VerifyLaunch(test_case.activity_name, test_case.mime_type,
                 test_case.files_to_open);
  }
}
