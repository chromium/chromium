// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_H_
#define CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/views/extensions/extension_dialog_observer.h"
#include "ui/gfx/native_widget_types.h"  // gfx::NativeWindow
#include "ui/shell_dialogs/select_file_dialog.h"

class ExtensionDialog;
class Profile;

namespace content {
class RenderViewHost;
class WebContents;
}

namespace ui {
struct SelectedFileInfo;
class SelectFilePolicy;
}

// Shows a dialog box for selecting a file or a folder, using the
// file manager extension implementation.
class SelectFileDialogExtension
    : public ui::SelectFileDialog,
      public ExtensionDialogObserver {
 public:
  // Opaque ID type for identifying the tab spawned each dialog, unique for
  // every WebContents or every Android task ID.
  typedef std::string RoutingID;

  static const int kAndroidTaskIdNone = -1;

  static SelectFileDialogExtension* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy);

  // BaseShellDialog implementation.
  bool IsRunning(gfx::NativeWindow owner_window) const override;
  void ListenerDestroyed() override;

  // ExtensionDialog::Observer implementation.
  void ExtensionDialogClosing(ExtensionDialog* dialog) override;
  void ExtensionTerminated(ExtensionDialog* dialog) override;

  // Routes callback to appropriate SelectFileDialog::Listener based on the
  // owning |web_contents|.
  static void OnFileSelected(RoutingID routing_id,
                             const ui::SelectedFileInfo& file,
                             int index);
  static void OnMultiFilesSelected(
      RoutingID routing_id,
      const std::vector<ui::SelectedFileInfo>& files);
  static void OnFileSelectionCanceled(RoutingID routing_id);

  // For testing, so we can inject JavaScript into the contained view.
  content::RenderViewHost* GetRenderViewHost();

  // Call SelectFile with params specific to Chrome OS file manager.
  // |owner_android_task_id| is the Android task ID of the owner window if the
  //     owner is Android, or kAndroidTaskIdNone if the owner is browser.
  // |show_android_picker_apps| determines whether to show Android picker apps
  //     in the select file dialog.
  void SelectFileWithFileManagerParams(
      Type type,
      const base::string16& title,
      const base::FilePath& default_path,
      const FileTypeInfo* file_types,
      int file_type_index,
      const base::FilePath::StringType& default_extension,
      gfx::NativeWindow owning_window,
      void* params,
      int owner_android_task_id,
      bool show_android_picker_apps);

 protected:
  // SelectFileDialog implementation.
  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override;

 private:
  friend class SelectFileDialogExtensionBrowserTest;
  friend class SelectFileDialogExtensionTest;
  friend class SelectFileDialogExtensionTestFactory;

  // Object is ref-counted, use Create().
  explicit SelectFileDialogExtension(
      SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy);
  ~SelectFileDialogExtension() override;

  // Invokes the appropriate file selection callback on our listener.
  void NotifyListener();

  // Adds this to the list of pending dialogs, used for testing.
  void AddPending(RoutingID routing_id);

  // Check if the list of pending dialogs contains dialog for |routing_id|.
  static bool PendingExists(RoutingID routing_id);

  // Returns true if the dialog has multiple file type choices.
  bool HasMultipleFileTypeChoicesImpl() override;

  // Returns true if |extension_dialog_| is resizable; the dialog must be
  // non-null at the time of this call.
  bool IsResizeable() const;

  bool has_multiple_file_type_choices_;

  // Host for the extension that implements this dialog.
  scoped_refptr<ExtensionDialog> extension_dialog_;

  // ID of the tab that spawned this dialog, used to route callbacks.
  RoutingID routing_id_;

  // Pointer to the profile the dialog is running in.
  Profile* profile_;

  // The window that created the dialog.
  gfx::NativeWindow owner_window_;

  // We defer the callback into SelectFileDialog::Listener until the window
  // closes, to match the semantics of file selection on Windows and Mac.
  // These are the data passed to the listener.
  enum SelectionType {
    CANCEL = 0,
    SINGLE_FILE,
    MULTIPLE_FILES
  };
  SelectionType selection_type_;
  std::vector<ui::SelectedFileInfo> selection_files_;
  int selection_index_;
  void* params_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogExtension);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_H_
