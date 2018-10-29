// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_SELECT_FILE_DIALOG_IMPL_GTK_H_
#define CHROME_BROWSER_UI_LIBGTKUI_SELECT_FILE_DIALOG_IMPL_GTK_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chrome/browser/ui/libgtkui/gtk_util.h"
#include "chrome/browser/ui/libgtkui/select_file_dialog_impl.h"
#include "ui/base/glib/glib_signal.h"

namespace libgtkui {

// Implementation of SelectFileDialog that shows a Gtk common dialog for
// choosing a file or folder. This acts as a modal dialog.
class SelectFileDialogImplGTK : public SelectFileDialogImpl,
                                public aura::WindowObserver {
 public:
  SelectFileDialogImplGTK(Listener* listener,
                          std::unique_ptr<ui::SelectFilePolicy> policy);

 protected:
  ~SelectFileDialogImplGTK() override;

  // BaseShellDialog implementation:
  bool IsRunning(gfx::NativeWindow parent_window) const override;

  // SelectFileDialog implementation.
  // |params| is user data we pass back via the Listener interface.
  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override;

 private:
  friend class FilePicker;

  struct WidgetData {
    WidgetData();
    ~WidgetData();

    // User data that we pass back to |listener_| once the result of the select
    // file/folder action is known.
    void* params = nullptr;

    aura::Window* parent = nullptr;

    std::unique_ptr<base::OnceClosure> enable_event_listening;
  };

  bool HasMultipleFileTypeChoicesImpl() override;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Add the filters from |file_types_| to |chooser|.
  void AddFilters(GtkFileChooser* chooser);

  // Notifies the listener that a single file was chosen.
  void FileSelected(GtkWidget* dialog, const base::FilePath& path);

  // Notifies the listener that multiple files were chosen.
  void MultiFilesSelected(GtkWidget* dialog,
                          const std::vector<base::FilePath>& files);

  // Notifies the listener that no file was chosen (the action was canceled).
  // Dialog is passed so we can find that |params| pointer that was passed to
  // us when we were told to show the dialog.
  void FileNotSelected(GtkWidget* dialog);

  GtkWidget* CreateSelectFolderDialog(Type type,
                                      const std::string& title,
                                      const base::FilePath& default_path,
                                      gfx::NativeWindow parent);

  GtkWidget* CreateFileOpenDialog(const std::string& title,
                                  const base::FilePath& default_path,
                                  gfx::NativeWindow parent);

  GtkWidget* CreateMultiFileOpenDialog(const std::string& title,
                                       const base::FilePath& default_path,
                                       gfx::NativeWindow parent);

  GtkWidget* CreateSaveAsDialog(const std::string& title,
                                const base::FilePath& default_path,
                                gfx::NativeWindow parent);

  // Returns the |params| associated with |dialog|.
  void* GetParamsForDialog(GtkWidget* dialog);

  // Check whether response_id corresponds to the user cancelling/closing the
  // dialog. Used as a helper for the below callbacks.
  bool IsCancelResponse(gint response_id);

  // Common function for OnSelectSingleFileDialogResponse and
  // OnSelectSingleFolderDialogResponse.
  void SelectSingleFileHelper(GtkWidget* dialog,
                              gint response_id,
                              bool allow_folder);

  // Common function for CreateFileOpenDialog and CreateMultiFileOpenDialog.
  GtkWidget* CreateFileOpenHelper(const std::string& title,
                                  const base::FilePath& default_path,
                                  gfx::NativeWindow parent);

  // Destroys the widget and deallocates all resources for dialogs_[dialog].
  void DestroyDialog(GtkWidget* dialog);

  // Deallocates all resources for dialogs_[dialog].
  void OnFileChooserDestroyInternal(GtkWidget* dialog);

  // The below callbacks may only be called from GTK, otherwise it's possible
  // that the keep_alive scoped_refptr's will double-destruct |this|
  // (https://crbug.com/897999).

  // Callback for when the user responds to a Save As or Open File dialog.
  CHROMEG_CALLBACK_1(SelectFileDialogImplGTK,
                     void,
                     OnSelectSingleFileDialogResponse,
                     GtkWidget*,
                     int);

  // Callback for when the user responds to a Select Folder dialog.
  CHROMEG_CALLBACK_1(SelectFileDialogImplGTK,
                     void,
                     OnSelectSingleFolderDialogResponse,
                     GtkWidget*,
                     int);

  // Callback for when the user responds to a Open Multiple Files dialog.
  CHROMEG_CALLBACK_1(SelectFileDialogImplGTK,
                     void,
                     OnSelectMultiFileDialogResponse,
                     GtkWidget*,
                     int);

  // Callback for when the file chooser gets destroyed.
  CHROMEG_CALLBACK_0(SelectFileDialogImplGTK,
                     void,
                     OnFileChooserDestroy,
                     GtkWidget*);

  // Callback for when we update the preview for the selection.
  CHROMEG_CALLBACK_0(SelectFileDialogImplGTK,
                     void,
                     OnUpdatePreview,
                     GtkWidget*);

  // The GtkImage widget for showing previews of selected images.
  GtkWidget* preview_;

  // All our dialogs.
  base::flat_map<GtkWidget*, std::unique_ptr<WidgetData>> dialogs_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImplGTK);
};

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_SELECT_FILE_DIALOG_IMPL_GTK_H_
