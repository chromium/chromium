// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_SELECT_FILE_DIALOG_EXTENSION_H_
#define CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_SELECT_FILE_DIALOG_EXTENSION_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/gfx/native_widget_types.h"  // gfx::NativeWindow
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

class Profile;

namespace aura {
class Window;
}  // namespace aura

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace ui {
struct SelectedFileInfo;
class SelectFilePolicy;
}  // namespace ui

// Shows a dialog box for selecting a file or a folder, using the
// file manager extension implementation.
class SelectFileDialogExtension : public ui::SelectFileDialog {
 public:
  // Opaque ID type for identifying the tab spawned each dialog, unique for
  // every WebContents or every Android task ID.
  typedef std::string RoutingID;

  SelectFileDialogExtension(const SelectFileDialogExtension&) = delete;
  SelectFileDialogExtension& operator=(SelectFileDialogExtension&) = delete;

  static SelectFileDialogExtension* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy);

  // ui::SelectFileDialog:
  bool IsRunning(gfx::NativeWindow owner_window) const override;
  void ListenerDestroyed() override;

  // Routes callback to appropriate SelectFileDialog::Listener based on the
  // owning |web_contents|.
  static void OnFileSelected(RoutingID routing_id,
                             const ui::SelectedFileInfo& file,
                             int index);
  static void OnMultiFilesSelected(
      RoutingID routing_id,
      const std::vector<ui::SelectedFileInfo>& files);
  static void OnFileSelectionCanceled(RoutingID routing_id);

  // Helper method that given parameters that are passed to the
  // SelectFileWithFileManagerParams method creates a URL for launching File
  // Manager as a dialog.
  static GURL MakeDialogURL(Type type,
                            const std::u16string& title,
                            const base::FilePath& default_path,
                            const FileTypeInfo* file_types,
                            int file_type_index,
                            const std::string& search_query,
                            bool show_android_picker_apps,
                            std::vector<std::string> volume_filter,
                            Profile* profile);

  // Allows access to the extension's main frame for injecting javascript.
  content::RenderFrameHost* GetPrimaryMainFrame();

  // Call SelectFile with params specific to Chrome OS file manager.
  // |owner| specifies the window and app type that opened the dialog.
  // |show_android_picker_apps| determines whether to show Android picker apps
  //     in the select file dialog.
  struct Owner {
    Owner();
    ~Owner();
    Owner(const Owner&);
    Owner& operator=(const Owner&);
    Owner(Owner&&);
    Owner& operator=(Owner&&);

    // The native window that opened the dialog.
    raw_ptr<aura::Window, LeakedDanglingUntriaged> window = nullptr;
    // Android task ID if the owner window is an Android app.
    std::optional<int> android_task_id;
    // Lacros window ID if the owner window is a Lacros browser. This field
    // can be nullopt even when is_lacros is true, for dialogs that are not
    // owned by a particular window, aka "modeless" dialog.
    std::optional<std::string> lacros_window_id;
    // Set to true only if SelectFileAsh opened the dialog.
    // TODO(crbug.com/369851375): Delete this; Lacros has sunset.
    bool is_lacros = false;
    // The URL or Component type of the caller that opened the dialog (Save
    // As/File Picker).
    std::optional<policy::DlpFileDestination> dialog_caller;
  };
  void SelectFileWithFileManagerParams(Type type,
                                       const std::u16string& title,
                                       const base::FilePath& default_path,
                                       const FileTypeInfo* file_types,
                                       int file_type_index,
                                       const Owner& owner,
                                       const std::string& search_query,
                                       bool show_android_picker_apps,
                                       bool use_media_store_filter = false);

 protected:
  // ui::SelectFileDialog:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override;
  bool HasMultipleFileTypeChoicesImpl() override;

 private:
  friend class BaseSelectFileDialogExtensionBrowserTest;
  friend class SelectFileDialogExtensionTest;
  friend class SelectFileDialogExtensionTestFactory;
  friend class SystemFilesAppDialogDelegate;
  FRIEND_TEST_ALL_PREFIXES(SelectFileDialogExtensionTest, FileSelected);
  FRIEND_TEST_ALL_PREFIXES(SelectFileDialogExtensionTest,
                           FileSelectionCanceled);
  FRIEND_TEST_ALL_PREFIXES(SelectFileDialogExtensionTest, SelfDeleting);
  FRIEND_TEST_ALL_PREFIXES(SelectFileDialogExtensionBrowserTest,
                           DialogCallerSetWhenPassed);

  // For the benefit of SystemFilesAppDialogDelegate.
  void OnSystemDialogShown(content::WebContents* content,
                           const std::string& id);
  void OnSystemDialogWillClose();

  // Object is ref-counted, use Create().
  explicit SelectFileDialogExtension(
      SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy);
  ~SelectFileDialogExtension() override;

  // Applies DLP policies if there's any, then notifies listeners accordingly.
  void ApplyPolicyAndNotifyListener(
      std::optional<policy::DlpFileDestination> dialog_caller);

  // Invokes the appropriate file selection callback on our listener.
  void NotifyListener(std::vector<ui::SelectedFileInfo> selection_files);

  // Adds this to the list of pending dialogs, used for testing.
  void AddPending(RoutingID routing_id);

  // Check if the list of pending dialogs contains dialog for |routing_id|.
  static bool PendingExists(RoutingID routing_id);

  // Returns true if |extension_dialog_| is resizable; the dialog must be
  // non-null at the time of this call.
  bool IsResizeable() const;

  bool has_multiple_file_type_choices_ = false;

  // If System Files App is enabled it stores the web contents associated with
  // System File App dialog. Not owned by this class. Set only while System
  // Files App dialog is opened.
  raw_ptr<content::WebContents, LeakedDanglingUntriaged>
      system_files_app_web_contents_;

  // ID of the tab that spawned this dialog, used to route callbacks.
  RoutingID routing_id_;

  // Pointer to the profile the dialog is running in.
  raw_ptr<Profile, LeakedDanglingUntriaged> profile_ = nullptr;

  // Information about the dialog's owner, such as the window or app type.
  Owner owner_;

  // Dialog type.
  Type type_;

  // We defer the callback into SelectFileDialog::Listener until the window
  // closes, to match the semantics of file selection on Windows and Mac.
  // These are the data passed to the listener.
  enum SelectionType {
    CANCEL = 0,
    SINGLE_FILE,
    MULTIPLE_FILES
  };
  SelectionType selection_type_ = CANCEL;
  std::vector<ui::SelectedFileInfo> selection_files_;
  int selection_index_ = 0;
  bool can_resize_ = true;
  base::WeakPtrFactory<SelectFileDialogExtension> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_SELECT_FILE_DIALOG_EXTENSION_H_
