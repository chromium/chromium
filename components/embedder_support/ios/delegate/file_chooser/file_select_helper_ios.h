// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_FILE_CHOOSER_FILE_SELECT_HELPER_IOS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_FILE_CHOOSER_FILE_SELECT_HELPER_IOS_H_

#include <memory>
#include <vector>

#include "content/public/browser/browser_thread.h"
#include "net/base/directory_lister.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class FileSelectListener;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace ui {
struct SelectedFileInfo;
}  // namespace ui

namespace web_contents_delegate_ios {

class FileSelectHelperIOS
    : public base::RefCountedThreadSafe<
          FileSelectHelperIOS,
          content::BrowserThread::DeleteOnUIThread>,
      public ui::SelectFileDialog::Listener,
      public net::DirectoryLister::DirectoryListerDelegate {
 public:
  // Creates a file chooser dialog.
  static void RunFileChooser(
      content::RenderFrameHost* render_frame_host,
      scoped_refptr<content::FileSelectListener> listener,
      const blink::mojom::FileChooserParams& params);

  FileSelectHelperIOS(const FileSelectHelperIOS&) = delete;
  FileSelectHelperIOS& operator=(const FileSelectHelperIOS&) = delete;

 private:
  friend class base::RefCountedThreadSafe<FileSelectHelperIOS>;
  friend class base::DeleteHelper<FileSelectHelperIOS>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;

  FileSelectHelperIOS();
  ~FileSelectHelperIOS() override;

  // SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;

  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override;

  void FileSelectionCanceled() override;

  // net::DirectoryLister::DirectoryListerDelegate:
  void OnListFile(
      const net::DirectoryLister::DirectoryListerData& data) override;
  void OnListDone(int error) override;

  // Create a file selection dialog actually when it's called by static
  // RunFileChooser method.
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      blink::mojom::FileChooserParamsPtr params);

  // Cleans up and releases this instance. This must be called after the last
  // callback is received from the file chooser dialog.
  void RunFileChooserEnd();

  // This method is called after the user has chosen the file(s) in the UI in
  // order to process and filter the list before returning the final result to
  // the caller.
  void ConvertToFileChooserFileInfoList(
      const std::vector<ui::SelectedFileInfo>& files);

  // Kicks off a new directory enumeration.
  void StartNewEnumeration(const base::FilePath& path);

  base::FilePath base_dir_;

  // Maintain an active directory enumeration. These could come from the file
  // select dialog or from drag-and-drop of directories.  There could not be
  // more than one going on at a time.
  struct ActiveDirectoryEnumeration;
  std::unique_ptr<ActiveDirectoryEnumeration> directory_enumeration_;

  // A weak pointer to check the life of the WebContents of the RenderFrameHost.
  base::WeakPtr<content::WebContents> web_contents_;

  // A listener to receive the result of FileSelectHelperIOS class.
  scoped_refptr<content::FileSelectListener> listener_;

  // A dialog box used for choosing files to upload from file form fields.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  std::unique_ptr<ui::SelectFileDialog::FileTypeInfo> select_file_types_;

  // The type of the file dialog last shown.
  ui::SelectFileDialog::Type dialog_type_ =
      ui::SelectFileDialog::SELECT_OPEN_FILE;

  // The mode of the file dialog last shown.
  blink::mojom::FileChooserParams::Mode dialog_mode_ =
      blink::mojom::FileChooserParams::Mode::kOpen;
};

}  // namespace web_contents_delegate_ios

#endif  // COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_FILE_CHOOSER_FILE_SELECT_HELPER_IOS_H_
