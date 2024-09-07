// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension/select_file_dialog_extension.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/gfx/color_palette.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

class KeyboardVisibleWaiter : public ChromeKeyboardControllerClient::Observer {
 public:
  KeyboardVisibleWaiter() {
    ChromeKeyboardControllerClient::Get()->AddObserver(this);
  }
  ~KeyboardVisibleWaiter() override {
    ChromeKeyboardControllerClient::Get()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // ChromeKeyboardControllerClient::Observer
  void OnKeyboardVisibilityChanged(bool visible) override {
    if (visible)
      run_loop_.QuitWhenIdle();
  }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

class MockSelectFileDialogListener : public ui::SelectFileDialog::Listener {
 public:
  MockSelectFileDialogListener() = default;

  MockSelectFileDialogListener(const MockSelectFileDialogListener&) = delete;
  MockSelectFileDialogListener& operator=(const MockSelectFileDialogListener&) =
      delete;

  bool file_selected() const { return file_selected_; }
  bool canceled() const { return canceled_; }
  base::FilePath path() const { return path_; }

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override {
    file_selected_ = true;
    path_ = file.path();
    QuitMessageLoop();
  }
  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override {
    QuitMessageLoop();
  }
  void FileSelectionCanceled() override {
    canceled_ = true;
    QuitMessageLoop();
  }

  void WaitForCalled() {
    message_loop_runner_ = new content::MessageLoopRunner();
    message_loop_runner_->Run();
  }

 private:
  void QuitMessageLoop() {
    if (message_loop_runner_.get())
      message_loop_runner_->Quit();
  }

  bool file_selected_ = false;
  bool canceled_ = false;
  base::FilePath path_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

// Parametrization of tests. We run tests with and
// without filt type filter enabled, and in tablet mode and in regular mode.
struct TestMode {
  TestMode(bool file_type_filter, bool tablet_mode)
      : file_type_filter(file_type_filter), tablet_mode(tablet_mode) {}

  static testing::internal::ParamGenerator<TestMode> SystemWebAppValues() {
    return ::testing::Values(TestMode(false, false), TestMode(false, true),
                             TestMode(true, false), TestMode(true, true));
  }

  bool file_type_filter;
  bool tablet_mode;
};

// TODO(b/194969976): Print human readable test names instead TestName/1, etc.
class BaseSelectFileDialogExtensionBrowserTest
    : public extensions::ExtensionBrowserTest,
      public testing::WithParamInterface<TestMode> {
 public:
  BaseSelectFileDialogExtensionBrowserTest() {
    use_file_type_filter_ = GetParam().file_type_filter;
  }

  enum DialogButtonType {
    DIALOG_BTN_OK,
    DIALOG_BTN_CANCEL
  };

  void SetUp() override {
    // Create the dialog wrapper and listener objects.
    listener_ = std::make_unique<MockSelectFileDialogListener>();
    dialog_ = new SelectFileDialogExtension(listener_.get(), nullptr);

    // One mount point will be needed. Files app looks for the "Downloads"
    // volume mount point by default, so use that.
    base::FilePath tmp_path;
    base::PathService::Get(base::DIR_TEMP, &tmp_path);
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDirUnderPath(tmp_path));
    downloads_dir_ =
        tmp_dir_.GetPath().AppendASCII("My Files").AppendASCII("Downloads");
    base::CreateDirectory(downloads_dir_);

    // Must run after our setup because it actually runs the test.
    extensions::ExtensionBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    CHECK(profile());

    // Create a file system mount point for the "Downloads" directory.
    EXPECT_TRUE(
        file_manager::VolumeManager::Get(profile())
            ->RegisterDownloadsDirectoryForTesting(downloads_dir_.DirName()));
    profile()->GetPrefs()->SetFilePath(prefs::kDownloadDefaultDirectory,
                                       downloads_dir_);

    // The test resources are setup: enable and add default ChromeOS component
    // extensions now and not before: crbug.com/831074, crbug.com/804413.
    file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());
  }

  void TearDown() override {
    extensions::ExtensionBrowserTest::TearDown();

    // Delete the dialogs first since they hold a pointer to their listener.
    dialog_.reset();
    listener_.reset();
    second_dialog_.reset();
    second_listener_.reset();
  }

  void CheckJavascriptErrors() {
    content::RenderFrameHost* host = dialog_->GetPrimaryMainFrame();
    ASSERT_EQ(0, content::EvalJs(host, "window.JSErrorCount"));
  }

  void ClickElement(const std::string& selector) {
    content::RenderFrameHost* frame_host = dialog_->GetPrimaryMainFrame();

    auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
    CHECK(web_contents);

    int x = content::EvalJs(web_contents,
                            "var bounds = document.querySelector('" + selector +
                                "').getBoundingClientRect();"
                                "Math.floor(bounds.left + bounds.width / 2);")
                .ExtractInt();

    int y = content::EvalJs(web_contents,
                            "var bounds = document.querySelector('" + selector +
                                "').getBoundingClientRect();"
                                "Math.floor(bounds.top + bounds.height / 2);")
                .ExtractInt();

    LOG(INFO) << "ClickElement " << selector << " (" << x << "," << y << ")";
    constexpr auto kButton = blink::WebMouseEvent::Button::kLeft;
    content::SimulateMouseClickAt(web_contents, 0, kButton, gfx::Point(x, y));
  }

  void OpenDialog(ui::SelectFileDialog::Type dialog_type,
                  const base::FilePath& file_path,
                  const gfx::NativeWindow& owning_window,
                  const std::string& additional_message,
                  const GURL* caller = nullptr) {
    if (GetParam().tablet_mode) {
      ash::ShellTestApi().SetTabletModeEnabledForTest(true);
    }
    // Open the file dialog: Files app will signal that it is loaded via the
    // "ready" chrome.test.sendMessage().
    ExtensionTestMessageListener init_listener("ready");

    std::unique_ptr<ExtensionTestMessageListener> additional_listener;
    if (!additional_message.empty()) {
      additional_listener =
          std::make_unique<ExtensionTestMessageListener>(additional_message);
    }

    std::u16string title;
    // Include a file type filter. This triggers additional functionality within
    // the Files app.
    ui::SelectFileDialog::FileTypeInfo file_types{{FILE_PATH_LITERAL("html")}};
    const ui::SelectFileDialog::FileTypeInfo* file_types_ptr =
        UseFileTypeFilter() ? &file_types : nullptr;

    dialog_->SelectFile(dialog_type, title, file_path, file_types_ptr, 0,
                        FILE_PATH_LITERAL(""), owning_window, caller);
    LOG(INFO) << "Waiting for JavaScript ready message.";
    ASSERT_TRUE(init_listener.WaitUntilSatisfied());

    if (additional_listener.get()) {
      LOG(INFO) << "Waiting for JavaScript " << additional_message
                << " message.";
      ASSERT_TRUE(additional_listener->WaitUntilSatisfied());
    }

    // Dialog should be running now.
    ASSERT_TRUE(dialog_->IsRunning(owning_window));

    ASSERT_NO_FATAL_FAILURE(CheckJavascriptErrors());
  }

  bool OpenDialogIsResizable() const {
    return dialog_->IsResizeable();  // See crrev.com/600185.
  }

  void TryOpeningSecondDialog(const gfx::NativeWindow& owning_window) {
    second_listener_ = std::make_unique<MockSelectFileDialogListener>();
    second_dialog_ =
        new SelectFileDialogExtension(second_listener_.get(), nullptr);

    // The dialog type is not relevant for this test but is required: use the
    // open file dialog type.
    second_dialog_->SelectFile(
        ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string() /* title */,
        base::FilePath() /* default_path */, nullptr /* file_types */,
        0 /* file_type_index */, FILE_PATH_LITERAL("") /* default_extension */,
        owning_window);
  }

  void ClickJsButton(content::RenderFrameHost* frame_host,
                     DialogButtonType button_type) {
    std::string button_class =
        (button_type == DIALOG_BTN_OK) ? ".button-panel .ok" :
                                         ".button-panel .cancel";
    std::u16string script = base::ASCIIToUTF16(
        "console.log(\'Test JavaScript injected.\');"
        "document.querySelector(\'" +
        button_class + "\').click();");
    // The file selection handler code closes the dialog but does not return
    // control to JavaScript, so do not wait for the script return value.
    frame_host->ExecuteJavaScriptForTests(script, base::NullCallback(),
                                          content::ISOLATED_WORLD_ID_GLOBAL);
  }

  void CloseDialog(DialogButtonType button_type,
                   const gfx::NativeWindow& owning_window) {
    // Inject JavaScript into the dialog to click the dialog |button_type|.
    content::RenderFrameHost* frame_host = dialog_->GetPrimaryMainFrame();

    ClickJsButton(frame_host, button_type);

    // Instead, wait for Listener notification that the window has closed.
    LOG(INFO) << "Waiting for window close notification.";
    listener_->WaitForCalled();

    // Dialog no longer believes it is running.
    if (owning_window)
      ASSERT_FALSE(dialog_->IsRunning(owning_window));
  }

  bool UseFileTypeFilter() { return use_file_type_filter_; }

  base::ScopedTempDir tmp_dir_;
  base::FilePath downloads_dir_;

  std::unique_ptr<MockSelectFileDialogListener> listener_;
  scoped_refptr<SelectFileDialogExtension> dialog_;

  std::unique_ptr<MockSelectFileDialogListener> second_listener_;
  scoped_refptr<SelectFileDialogExtension> second_dialog_;

  bool use_file_type_filter_;
};

// Tests FileDialog with and without file filter.
class SelectFileDialogExtensionBrowserTest
    : public BaseSelectFileDialogExtensionBrowserTest {};

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest, CreateAndDestroy) {
  // The browser window must exist for us to test dialog's parent window.
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Before we call SelectFile, the dialog should not be running/visible.
  ASSERT_FALSE(dialog_->IsRunning(owning_window));
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest, DestroyListener) {
  // Some users of SelectFileDialog destroy their listener before cleaning
  // up the dialog.  Make sure we don't crash.
  dialog_->ListenerDestroyed();
  listener_.reset();
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest, DestroyListener2) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Open the file dialog on the default path.
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     base::FilePath(), owning_window, ""));

  // Get the Files app WebContents/Framehost, before deleting the dialog_.
  content::RenderFrameHost* frame_host = dialog_->GetPrimaryMainFrame();

  // Some users of SelectFileDialog destroy their listener before cleaning
  // up the dialog, delete the `dialog_`, however the
  // SystemFilesAppDialogDelegate will still be alive with the Files app
  // WebContents.  Make sure we don't crash.
  dialog_->ListenerDestroyed();
  listener_.reset();
  dialog_.reset();

  // This will close the FrameHost/WebContents and will try to close the
  // `dialog_`.
  ClickJsButton(frame_host, DIALOG_BTN_CANCEL);

  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest, CanResize) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Open the file dialog on the default path.
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     base::FilePath(), owning_window, ""));

  // The dialog should be resizable.
  ASSERT_EQ(!GetParam().tablet_mode, OpenDialogIsResizable());

  // Click the "Cancel" button. This closes the dialog thus removing it from
  // `PendingDialog::map_`. `PendingDialog::map_` otherwise prevents the dialog
  // from being destroyed on `reset()` in test TearDown and the
  // `SelectFileDialog::listener_` becomes dangling.
  CloseDialog(DIALOG_BTN_CANCEL, owning_window);
}


IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest,
                       SelectFileAndCancel) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Open the file dialog on the default path.
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     base::FilePath(), owning_window, ""));
  // Click the "Cancel" button.
  CloseDialog(DIALOG_BTN_CANCEL, owning_window);

  // Listener should have been informed of the cancellation.
  ASSERT_FALSE(listener_->file_selected());
  ASSERT_TRUE(listener_->canceled());
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest,
                       SelectFileAndOpen) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Create an empty file to provide the file to open.
  const base::FilePath test_file =
      downloads_dir_.AppendASCII("file_manager_test.html");
  {
    base::ScopedAllowBlockingForTesting allow_io;
    FILE* fp = base::OpenFile(test_file, "w");
    ASSERT_TRUE(fp != nullptr);
    ASSERT_TRUE(base::CloseFile(fp));
  }

  // Open the file dialog, providing a path to the file to open so the dialog
  // will automatically select it.  Ensure the "Open" button is enabled by
  // waiting for notification from chrome.test.sendMessage().
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     test_file, owning_window, "dialog-ready"));
  // Click the "Open" button.
  CloseDialog(DIALOG_BTN_OK, owning_window);

  // Listener should have been informed that the file was opened.
  ASSERT_TRUE(listener_->file_selected());
  ASSERT_FALSE(listener_->canceled());
  ASSERT_EQ(test_file, listener_->path());
}

// TODO(crbug.com/40249076): Re-enable this test
IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest,
                       DISABLED_SelectFileAndSave) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Open the file dialog to save a file, providing a suggested file path.
  // Ensure the "Save" button is enabled by waiting for notification from
  // chrome.test.sendMessage().
  const base::FilePath test_file =
      downloads_dir_.AppendASCII("file_manager_save.html");
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                                     test_file, owning_window, "dialog-ready"));
  // Click the "Save" button.
  CloseDialog(DIALOG_BTN_OK, owning_window);

  // Listener should have been informed that the file was saved.
  ASSERT_TRUE(listener_->file_selected());
  ASSERT_FALSE(listener_->canceled());
  ASSERT_EQ(test_file, listener_->path());
}

// TODO(crbug.com/40249076): Re-enable this test
IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest,
                       DISABLED_SelectFileVirtualKeyboard) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Enable the virtual keyboard.
  ash::ShellTestApi().EnableVirtualKeyboard();

  auto* client = ChromeKeyboardControllerClient::Get();
  EXPECT_FALSE(client->is_keyboard_visible());

  // Open the file dialog to save a file, providing a suggested file path.
  // Ensure the "Save" button is enabled by waiting for notification from
  // chrome.test.sendMessage().
  const base::FilePath test_file =
      downloads_dir_.AppendASCII("file_manager_save.html");
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                                     test_file, owning_window, "dialog-ready"));

  // Click the dialog's filename input element.
  ASSERT_NO_FATAL_FAILURE(ClickElement("#filename-input-textbox"));

  // The virtual keyboard should be shown.
  KeyboardVisibleWaiter().Wait();
  EXPECT_TRUE(client->is_keyboard_visible());
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest,
                       OpenSingletonTabAndCancel) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Open the file dialog on the default path.
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     base::FilePath(), owning_window, ""));

  // Open a singleton tab in background.
  NavigateParams p(browser(), GURL("http://www.google.com"),
                   ui::PAGE_TRANSITION_LINK);
  p.window_action = NavigateParams::SHOW_WINDOW;
  p.disposition = WindowOpenDisposition::SINGLETON_TAB;
  Navigate(&p);

  // Click the "Cancel" button.
  CloseDialog(DIALOG_BTN_CANCEL, owning_window);

  // Listener should have been informed of the cancellation.
  ASSERT_FALSE(listener_->file_selected());
  ASSERT_TRUE(listener_->canceled());
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest, OpenTwoDialogs) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Open the file dialog on the default path.
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     base::FilePath(), owning_window, ""));

  // Requests to open a second file dialog should fail.
  TryOpeningSecondDialog(owning_window);
  ASSERT_FALSE(second_dialog_->IsRunning(owning_window));

  // Click the "Cancel" button.
  CloseDialog(DIALOG_BTN_CANCEL, owning_window);

  // Listener should have been informed of the cancellation.
  ASSERT_FALSE(listener_->file_selected());
  ASSERT_TRUE(listener_->canceled());
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest, FileInputElement) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Start the embedded test server.
  base::FilePath source_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
  auto test_data_dir = source_dir.AppendASCII("chrome")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("chromeos")
                           .AppendASCII("file_manager");
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate the browser to the file input element test page.
  const GURL url = embedded_test_server()->GetURL("/file_input/element.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(url, web_contents->GetLastCommittedURL());

  // Create a listener for the file dialog's "ready" message.
  ExtensionTestMessageListener listener("ready");

  // Click the file <input> element to open the file dialog.
  constexpr auto kButton = blink::WebMouseEvent::Button::kLeft;
  content::SimulateMouseClickAt(web_contents, 0, kButton, gfx::Point(0, 0));

  // Wait for file dialog's "ready" message.
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest,
                       OpenDialogWithoutOwningWindow) {
  gfx::NativeWindow owning_window = gfx::NativeWindow();

  // Open the file dialog with no |owning_window|.
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     base::FilePath(), owning_window, ""));

  // Click the "Cancel" button.
  CloseDialog(DIALOG_BTN_CANCEL, owning_window);

  // Listener should have been informed of the cancellation.
  ASSERT_TRUE(listener_->canceled());
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest, MultipleOpenFile) {
  // No use-after-free when Browser::OpenFile is called multiple times.
  browser()->OpenFile();
  browser()->OpenFile();
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionBrowserTest,
                       DialogCallerSetWhenPassed) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  const std::string url = "https://example.com/";
  const GURL caller = GURL(url);

  // Open the file dialog on the default path.
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     base::FilePath(), owning_window, "",
                                     &caller));

  // Check that the caller field is set correctly.
  ASSERT_TRUE(dialog_->owner_.dialog_caller.has_value());
  ASSERT_EQ(dialog_->owner_.dialog_caller->url().value(), url);

  // Click the "Cancel" button.
  CloseDialog(DIALOG_BTN_CANCEL, owning_window);

  // Listener should have been informed of the cancellation.
  ASSERT_FALSE(listener_->file_selected());
  ASSERT_TRUE(listener_->canceled());
}

INSTANTIATE_TEST_SUITE_P(SystemWebApp,
                         SelectFileDialogExtensionBrowserTest,
                         TestMode::SystemWebAppValues());

// Tests that ash window has correct colors for GM2.
// TODO(adanilo) factor out the unnecessary override of Setup().
class SelectFileDialogExtensionFlagTest
    : public BaseSelectFileDialogExtensionBrowserTest {
  void SetUp() override {
    BaseSelectFileDialogExtensionBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionFlagTest,
                       DialogColoredTitle_Light) {
  ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Open the file dialog on the default path.
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     base::FilePath(), owning_window, ""));
  content::RenderFrameHost* frame_host = dialog_->GetPrimaryMainFrame();
  aura::Window* dialog_window =
      frame_host->GetNativeView()->GetToplevelWindow();
  // This is cros_tokens::kDialogTitleBarColorLight
  SkColor dialog_title_bar_color = SkColorSetRGB(0xDF, 0xE0, 0xE1);
  EXPECT_EQ(dialog_window->GetProperty(chromeos::kFrameActiveColorKey),
            dialog_title_bar_color);
  EXPECT_EQ(dialog_window->GetProperty(chromeos::kFrameInactiveColorKey),
            dialog_title_bar_color);

  CloseDialog(DIALOG_BTN_CANCEL, owning_window);
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionFlagTest,
                       DialogColoredTitle_Dark) {
  ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Open the file dialog on the default path.
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     base::FilePath(), owning_window, ""));
  content::RenderFrameHost* frame_host = dialog_->GetPrimaryMainFrame();
  aura::Window* dialog_window =
      frame_host->GetNativeView()->GetToplevelWindow();
  // This is cros_tokens::kDialogTitleBarColorDark
  SkColor dialog_title_bar_color = SkColorSetRGB(0x4D, 0x4D, 0x50);
  EXPECT_EQ(dialog_window->GetProperty(chromeos::kFrameActiveColorKey),
            dialog_title_bar_color);
  EXPECT_EQ(dialog_window->GetProperty(chromeos::kFrameInactiveColorKey),
            dialog_title_bar_color);

  CloseDialog(DIALOG_BTN_CANCEL, owning_window);
}

INSTANTIATE_TEST_SUITE_P(SystemWebApp,
                         SelectFileDialogExtensionFlagTest,
                         TestMode::SystemWebAppValues());

using SelectFileDialogExtensionDarkLightModeEnabledTest =
    BaseSelectFileDialogExtensionBrowserTest;

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionDarkLightModeEnabledTest,
                       ColorModeChange) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Open the file dialog on the default path.
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     base::FilePath(), owning_window, ""));
  content::RenderFrameHost* frame_host = dialog_->GetPrimaryMainFrame();
  aura::Window* dialog_window =
      frame_host->GetNativeView()->GetToplevelWindow();

  auto* dark_light_mode_controller = ash::DarkLightModeController::Get();
  bool dark_mode_enabled = dark_light_mode_controller->IsDarkModeEnabled();
  SkColor initial_active_color =
      dialog_window->GetProperty(chromeos::kFrameActiveColorKey);
  SkColor initial_inactive_color =
      dialog_window->GetProperty(chromeos::kFrameInactiveColorKey);
  Profile* profile = chrome_test_utils::GetProfile(this);
  PrefService* prefs = profile->GetPrefs();

  // Switch the color mode.
  prefs->SetBoolean(ash::prefs::kDarkModeEnabled, !dark_mode_enabled);
  EXPECT_EQ(!dark_mode_enabled,
            dark_light_mode_controller->IsDarkModeEnabled());

  // Active and inactive colors in the other mode should be different from the
  // initial mode.
  EXPECT_NE(dialog_window->GetProperty(chromeos::kFrameActiveColorKey),
            initial_active_color);
  EXPECT_NE(dialog_window->GetProperty(chromeos::kFrameInactiveColorKey),
            initial_inactive_color);

  CloseDialog(DIALOG_BTN_CANCEL, owning_window);
}

INSTANTIATE_TEST_SUITE_P(SystemWebApp,
                         SelectFileDialogExtensionDarkLightModeEnabledTest,
                         TestMode::SystemWebAppValues());

// Tests applying policies before notifying listeners.
class SelectFileDialogExtensionPolicyTest
    : public BaseSelectFileDialogExtensionBrowserTest {
 protected:
  class MockFilesController : public policy::DlpFilesControllerAsh {
   public:
    explicit MockFilesController(const policy::DlpRulesManager& rules_manager,
                                 Profile* profile)
        : DlpFilesControllerAsh(rules_manager, profile) {}
    ~MockFilesController() override = default;

    MOCK_METHOD(void,
                CheckIfDownloadAllowed,
                (const policy::DlpFileDestination&,
                 const base::FilePath&,
                 base::OnceCallback<void(bool)>),
                (override));
    MOCK_METHOD(void,
                FilterDisallowedUploads,
                (std::vector<ui::SelectedFileInfo>,
                 const policy::DlpFileDestination&,
                 base::OnceCallback<void(std::vector<ui::SelectedFileInfo>)>),
                (override));
  };

  void TearDownOnMainThread() override {
    // Make sure the rules manager does not return a freed files controller.
    ON_CALL(*rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(nullptr));

    // The files controller must be destroyed before the profile since it's
    // holding a pointer to it.
    mock_files_controller_.reset();
    BaseSelectFileDialogExtensionBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::NiceMock<policy::MockDlpRulesManager>>(
            Profile::FromBrowserContext(context));
    rules_manager_ = dlp_rules_manager.get();

    mock_files_controller_ = std::make_unique<MockFilesController>(
        *rules_manager_, Profile::FromBrowserContext(context));
    ON_CALL(*rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(mock_files_controller_.get()));

    return dlp_rules_manager;
  }

  void SetupRulesManager() {
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(
                       &SelectFileDialogExtensionPolicyTest::SetDlpRulesManager,
                       base::Unretained(this)));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());

    ON_CALL(*rules_manager_, IsFilesPolicyEnabled)
        .WillByDefault(testing::Return(true));
  }

  raw_ptr<policy::MockDlpRulesManager, DanglingUntriaged> rules_manager_ =
      nullptr;
  std::unique_ptr<MockFilesController> mock_files_controller_ = nullptr;
  raw_ptr<storage::ExternalMountPoints> mount_points_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionPolicyTest, DlpDownloadAllow) {
  SetupRulesManager();

  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  const std::string url = "https://example.com/";
  const GURL caller = GURL(url);

  // Open the file dialog to save a file, providing a suggested file path.
  // Ensure the "Save" button is enabled by waiting for notification from
  // chrome.test.sendMessage().
  const base::FilePath test_file =
      downloads_dir_.AppendASCII("file_manager_save.html");
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                                     test_file, owning_window, "dialog-ready",
                                     &caller));

  EXPECT_CALL(
      *mock_files_controller_.get(),
      CheckIfDownloadAllowed(policy::DlpFileDestination(caller), test_file,
                             base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(true));

  // Click the "Save" button.
  CloseDialog(DIALOG_BTN_OK, owning_window);

  // Listener should have been informed of the selection.
  ASSERT_TRUE(listener_->file_selected());
  ASSERT_FALSE(listener_->canceled());
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionPolicyTest, DlpDownloadBlock) {
  SetupRulesManager();

  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  const std::string url = "https://example.com/";
  const GURL caller = GURL(url);

  // Open the file dialog to save a file, providing a suggested file path.
  // Ensure the "Save" button is enabled by waiting for notification from
  // chrome.test.sendMessage().
  const base::FilePath test_file =
      downloads_dir_.AppendASCII("file_manager_save.html");
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                                     test_file, owning_window, "dialog-ready",
                                     &caller));

  EXPECT_CALL(
      *mock_files_controller_.get(),
      CheckIfDownloadAllowed(policy::DlpFileDestination(caller), test_file,
                             base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(false));

  // Click the "Save" button.
  CloseDialog(DIALOG_BTN_OK, owning_window);

  // Listener should have been informed of the cancellation.
  ASSERT_FALSE(listener_->file_selected());
  ASSERT_TRUE(listener_->canceled());
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionPolicyTest, DlpUploadAllow) {
  SetupRulesManager();

  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Create an empty file to provide the file to open.
  const base::FilePath test_file =
      downloads_dir_.AppendASCII("file_manager_test.html");
  {
    base::ScopedAllowBlockingForTesting allow_io;
    FILE* fp = base::OpenFile(test_file, "w");
    ASSERT_TRUE(fp != nullptr);
    ASSERT_TRUE(base::CloseFile(fp));
  }
  base::FilePath test_file_virtual_path;
  ASSERT_TRUE(storage::ExternalMountPoints::GetSystemInstance()->GetVirtualPath(
      test_file, &test_file_virtual_path));

  const std::string url = "https://example.com/";
  const GURL caller = GURL(url);

  // Open the file dialog, providing a path to the file to open so the dialog
  // will automatically select it.  Ensure the "Open" button is enabled by
  // waiting for notification from chrome.test.sendMessage().
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     test_file, owning_window, "dialog-ready",
                                     &caller));

  std::vector<ui::SelectedFileInfo> selected_files;
  auto selected_file = ui::SelectedFileInfo(test_file);
  selected_file.virtual_path = test_file_virtual_path;
  selected_files.push_back(std::move(selected_file));
  EXPECT_CALL(*mock_files_controller_.get(),
              FilterDisallowedUploads(selected_files,
                                      policy::DlpFileDestination(caller),
                                      base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(selected_files));

  // Click the "Save" button.
  CloseDialog(DIALOG_BTN_OK, owning_window);

  // Listener should have been informed of the selection.
  ASSERT_TRUE(listener_->file_selected());
  ASSERT_FALSE(listener_->canceled());
}

IN_PROC_BROWSER_TEST_P(SelectFileDialogExtensionPolicyTest, DlpUploadBlock) {
  SetupRulesManager();

  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();
  ASSERT_NE(nullptr, owning_window);

  // Create an empty file to provide the file to open.
  const base::FilePath test_file =
      downloads_dir_.AppendASCII("file_manager_test.html");
  {
    base::ScopedAllowBlockingForTesting allow_io;
    FILE* fp = base::OpenFile(test_file, "w");
    ASSERT_TRUE(fp != nullptr);
    ASSERT_TRUE(base::CloseFile(fp));
  }
  base::FilePath test_file_virtual_path;
  ASSERT_TRUE(storage::ExternalMountPoints::GetSystemInstance()->GetVirtualPath(
      test_file, &test_file_virtual_path));

  const std::string url = "https://example.com/";
  const GURL caller = GURL(url);

  // Open the file dialog, providing a path to the file to open so the dialog
  // will automatically select it.  Ensure the "Open" button is enabled by
  // waiting for notification from chrome.test.sendMessage().
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     test_file, owning_window, "dialog-ready",
                                     &caller));

  std::vector<ui::SelectedFileInfo> selected_files;
  auto selected_file = ui::SelectedFileInfo(test_file);
  selected_file.virtual_path = test_file_virtual_path;
  selected_files.push_back(std::move(selected_file));
  EXPECT_CALL(*mock_files_controller_.get(),
              FilterDisallowedUploads(std::move(selected_files),
                                      policy::DlpFileDestination(caller),
                                      base::test::IsNotNullCallback()))
      .WillOnce(
          base::test::RunOnceCallback<2>(std::vector<ui::SelectedFileInfo>()));

  // Click the "Save" button.
  CloseDialog(DIALOG_BTN_OK, owning_window);

  // Listener should have been informed of the cancellation.
  ASSERT_FALSE(listener_->file_selected());
  ASSERT_TRUE(listener_->canceled());
}

INSTANTIATE_TEST_SUITE_P(SystemWebApp,
                         SelectFileDialogExtensionPolicyTest,
                         TestMode::SystemWebAppValues());
