// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension.h"

#include <memory>

#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/shell_test_api.mojom.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/tablet_mode_client_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/test/keyboard_test_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

class MockSelectFileDialogListener : public ui::SelectFileDialog::Listener {
 public:
  MockSelectFileDialogListener()
    : file_selected_(false),
      canceled_(false),
      params_(NULL) {
  }

  bool file_selected() const { return file_selected_; }
  bool canceled() const { return canceled_; }
  base::FilePath path() const { return path_; }
  void* params() const { return params_; }

  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override {
    file_selected_ = true;
    path_ = path;
    params_ = params;
    QuitMessageLoop();
  }
  void FileSelectedWithExtraInfo(const ui::SelectedFileInfo& selected_file_info,
                                 int index,
                                 void* params) override {
    FileSelected(selected_file_info.local_path, index, params);
  }
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override {
    QuitMessageLoop();
  }
  void FileSelectionCanceled(void* params) override {
    canceled_ = true;
    params_ = params;
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

  bool file_selected_;
  bool canceled_;
  base::FilePath path_;
  void* params_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockSelectFileDialogListener);
};

class SelectFileDialogExtensionBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  enum DialogButtonType {
    DIALOG_BTN_OK,
    DIALOG_BTN_CANCEL
  };

  void SetUp() override {
    // Create the dialog wrapper and listener objects.
    listener_.reset(new MockSelectFileDialogListener());
    dialog_ = new SelectFileDialogExtension(listener_.get(), NULL);

    // One mount point will be needed. Files app looks for the "Downloads"
    // volume mount point by default, so use that.
    base::FilePath tmp_path;
    base::PathService::Get(base::DIR_TEMP, &tmp_path);
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDirUnderPath(tmp_path));
    downloads_dir_ = tmp_dir_.GetPath().AppendASCII("Downloads");
    base::CreateDirectory(downloads_dir_);

    // Must run after our setup because it actually runs the test.
    extensions::ExtensionBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ash tablet mode does not automatically enable the virtual keyboard, so
    // force the virtual keyboard via the command line for tablet mode tests.
    const char* test_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();
    if (base::StringPiece(test_name).ends_with("_TabletMode"))
      command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);

    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    CHECK(profile());

    // Create a file system mount point for the "Downloads" directory.
    EXPECT_TRUE(file_manager::VolumeManager::Get(profile())
                    ->RegisterDownloadsDirectoryForTesting(downloads_dir_));
    profile()->GetPrefs()->SetFilePath(prefs::kDownloadDefaultDirectory,
                                       downloads_dir_);

    // The test resources are setup: enable and add default ChromeOS component
    // extensions now and not before: crbug.com/831074, crbug.com/804413.
    file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());
  }

  void TearDown() override {
    extensions::ExtensionBrowserTest::TearDown();

    // Delete the dialogs first since they hold a pointer to their listener.
    dialog_ = NULL;
    listener_.reset();
    second_dialog_ = NULL;
    second_listener_.reset();
  }

  void CheckJavascriptErrors() {
    content::RenderFrameHost* host =
        dialog_->GetRenderViewHost()->GetMainFrame();
    std::unique_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(host, "window.JSErrorCount");
    int js_error_count = 0;
    ASSERT_TRUE(value->GetAsInteger(&js_error_count));
    ASSERT_EQ(0, js_error_count);
  }

  void ClickElement(const std::string& selector) {
    content::RenderFrameHost* frame_host =
        dialog_->GetRenderViewHost()->GetMainFrame();

    auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
    CHECK(web_contents);

    int x;
    ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
        web_contents,
        "var bounds = document.querySelector('" + selector +
            "').getBoundingClientRect();"
            "domAutomationController.send("
            "    Math.floor(bounds.left + bounds.width / 2));",
        &x));

    int y;
    ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
        web_contents,
        "var bounds = document.querySelector('" + selector +
            "').getBoundingClientRect();"
            "domAutomationController.send("
            "    Math.floor(bounds.top + bounds.height / 2));",
        &y));

    LOG(INFO) << "ClickElement " << selector << " (" << x << "," << y << ")";
    constexpr auto kButton = blink::WebMouseEvent::Button::kLeft;
    content::SimulateMouseClickAt(web_contents, 0, kButton, gfx::Point(x, y));
  }

  void OpenDialog(ui::SelectFileDialog::Type dialog_type,
                  const base::FilePath& file_path,
                  const gfx::NativeWindow& owning_window,
                  const std::string& additional_message,
                  const bool check_js_errors = false) {
    // Open the file dialog: Files app will signal that it is loaded via the
    // "ready" chrome.test.sendMessage().
    const bool will_reply = false;
    ExtensionTestMessageListener init_listener("ready", will_reply);

    std::unique_ptr<ExtensionTestMessageListener> additional_listener;
    if (!additional_message.empty()) {
      additional_listener.reset(
          new ExtensionTestMessageListener(additional_message, will_reply));
    }

    dialog_->SelectFile(dialog_type,
                        base::string16() /* title */,
                        file_path,
                        NULL /* file_types */,
                         0 /* file_type_index */,
                        FILE_PATH_LITERAL("") /* default_extension */,
                        owning_window,
                        this /* params */);

    LOG(INFO) << "Waiting for JavaScript ready message.";
    ASSERT_TRUE(init_listener.WaitUntilSatisfied());

    if (additional_listener.get()) {
      LOG(INFO) << "Waiting for JavaScript " << additional_message
                << " message.";
      ASSERT_TRUE(additional_listener->WaitUntilSatisfied());
    }

    // Dialog should be running now.
    ASSERT_TRUE(dialog_->IsRunning(owning_window));

    if (check_js_errors) {
      // TODO(895703): Files app currently has errors during this call. Work
      // out why and either fix or remove this code.
      ASSERT_NO_FATAL_FAILURE(CheckJavascriptErrors());
    }
  }

  bool OpenDialogIsResizable() const {
    return dialog_->IsResizeable();  // See crrev.com/600185.
  }

  void TryOpeningSecondDialog(const gfx::NativeWindow& owning_window) {
    second_listener_.reset(new MockSelectFileDialogListener());
    second_dialog_ = new SelectFileDialogExtension(second_listener_.get(),
                                                   NULL);

    // The dialog type is not relevant for this test but is required: use the
    // open file dialog type.
    second_dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE,
                               base::string16() /* title */,
                               base::FilePath() /* default_path */,
                               NULL /* file_types */,
                               0 /* file_type_index */,
                               FILE_PATH_LITERAL("") /* default_extension */,
                               owning_window,
                               this /* params */);
  }

  void CloseDialog(DialogButtonType button_type,
                   const gfx::NativeWindow& owning_window) {
    // Inject JavaScript into the dialog to click the dialog |button_type|.
    content::RenderViewHost* host = dialog_->GetRenderViewHost();
    std::string button_class =
        (button_type == DIALOG_BTN_OK) ? ".button-panel .ok" :
                                         ".button-panel .cancel";
    base::string16 script = base::ASCIIToUTF16(
        "console.log(\'Test JavaScript injected.\');"
        "document.querySelector(\'" + button_class + "\').click();");
    // The file selection handler code closes the dialog but does not return
    // control to JavaScript, so do not wait for the script return value.
    host->GetMainFrame()->ExecuteJavaScriptForTests(script);

    // Instead, wait for Listener notification that the window has closed.
    LOG(INFO) << "Waiting for window close notification.";
    listener_->WaitForCalled();

    // Dialog no longer believes it is running.
    ASSERT_FALSE(dialog_->IsRunning(owning_window));
  }

  base::ScopedTempDir tmp_dir_;
  base::FilePath downloads_dir_;

  std::unique_ptr<MockSelectFileDialogListener> listener_;
  scoped_refptr<SelectFileDialogExtension> dialog_;

  std::unique_ptr<MockSelectFileDialogListener> second_listener_;
  scoped_refptr<SelectFileDialogExtension> second_dialog_;
};

IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest, CreateAndDestroy) {
  // Browser window must be up for us to test dialog window parent.
  gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
  ASSERT_TRUE(native_window != NULL);

  // Before we call SelectFile, dialog is not running/visible.
  ASSERT_FALSE(dialog_->IsRunning(native_window));
}

IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest, DestroyListener) {
  // Some users of SelectFileDialog destroy their listener before cleaning
  // up the dialog.  Make sure we don't crash.
  dialog_->ListenerDestroyed();
  listener_.reset();
}

IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest, CanResize) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();

  // Open the file dialog on the default path.
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     base::FilePath(), owning_window, ""));

  // The dialog should be resizable.
  ASSERT_TRUE(OpenDialogIsResizable());
}

IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest,
                       CanResize_TabletMode) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();

  // Setup tablet mode.
  test::SetAndWaitForTabletMode(true);

  // Open the file dialog on the default path.
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     base::FilePath(), owning_window, ""));

  // The dialog should not be resizable in tablet mode.
  ASSERT_FALSE(OpenDialogIsResizable());
}

// TODO(jamescook): Add a test for selecting a file for an <input type='file'/>
// page element, as that uses different memory management pathways.
// crbug.com/98791

IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest,
                       SelectFileAndCancel) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();

  // Open the file dialog on the default path.
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     base::FilePath(), owning_window, ""));
  // Click the "Cancel" button.
  CloseDialog(DIALOG_BTN_CANCEL, owning_window);

  // Listener should have been informed of the cancellation.
  ASSERT_FALSE(listener_->file_selected());
  ASSERT_TRUE(listener_->canceled());
  ASSERT_EQ(this, listener_->params());
}

IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest,
                       SelectFileAndOpen) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();

  // Create an empty file to provide the file to open.
  const base::FilePath test_file =
      downloads_dir_.AppendASCII("file_manager_test.html");
  FILE* fp = base::OpenFile(test_file, "w");
  ASSERT_TRUE(fp != NULL);
  ASSERT_TRUE(base::CloseFile(fp));

  // Open the file dialog, providing a path to the file to open so the dialog
  // will automatically select it.  Ensure the "Open" button is enabled by
  // waiting for notification from chrome.test.sendMessage().
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                     test_file, owning_window,
                                     "dialog-ready"));
  // Click the "Open" button.
  CloseDialog(DIALOG_BTN_OK, owning_window);

  // Listener should have been informed that the file was opened.
  ASSERT_TRUE(listener_->file_selected());
  ASSERT_FALSE(listener_->canceled());
  ASSERT_EQ(test_file, listener_->path());
  ASSERT_EQ(this, listener_->params());
}

IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest,
                       SelectFileAndSave) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();

  // Open the file dialog to save a file, providing a suggested file path.
  // Ensure the "Save" button is enabled by waiting for notification from
  // chrome.test.sendMessage().
  const base::FilePath test_file =
      downloads_dir_.AppendASCII("file_manager_save.html");
  ASSERT_NO_FATAL_FAILURE(OpenDialog(ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                                     test_file, owning_window,
                                     "dialog-ready"));
  // Click the "Save" button.
  CloseDialog(DIALOG_BTN_OK, owning_window);

  // Listener should have been informed that the file was saved.
  ASSERT_TRUE(listener_->file_selected());
  ASSERT_FALSE(listener_->canceled());
  ASSERT_EQ(test_file, listener_->path());
  ASSERT_EQ(this, listener_->params());
}

IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest,
                       SelectFileVirtualKeyboard_TabletMode) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();

  // Setup tablet mode.
  test::SetAndWaitForTabletMode(true);

  // Enable the virtual keyboard.
  ash::mojom::ShellTestApiPtr shell_test_api;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &shell_test_api);
  ash::mojom::ShellTestApiAsyncWaiter waiter(shell_test_api.get());
  waiter.EnableVirtualKeyboard();

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
  keyboard::WaitUntilShown();
  ASSERT_TRUE(keyboard::IsKeyboardShowing());
}

IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest,
                       OpenSingletonTabAndCancel) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();

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
  ASSERT_EQ(this, listener_->params());
}

IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest, OpenTwoDialogs) {
  gfx::NativeWindow owning_window = browser()->window()->GetNativeWindow();

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
  ASSERT_EQ(this, listener_->params());
}
