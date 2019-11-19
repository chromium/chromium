// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_VIEW_H_

#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/models/table_model.h"
#include "url/origin.h"

class NativeFileSystemUsageBubbleView : public LocationBarBubbleDelegateView {
 public:
  struct Usage {
    Usage();
    ~Usage();
    Usage(Usage&&);
    Usage& operator=(Usage&&);

    std::vector<base::FilePath> readable_directories;
    std::vector<base::FilePath> writable_files;
    std::vector<base::FilePath> writable_directories;
  };
  static void ShowBubble(content::WebContents* web_contents,
                         const url::Origin& origin,
                         Usage usage);

  // Closes the showing bubble (if one exists).
  static void CloseCurrentBubble();

  // Returns the bubble if the bubble is showing. Returns null otherwise.
  static NativeFileSystemUsageBubbleView* GetBubble();

 private:
  class FilePathListModel : public ui::TableModel {
   public:
    FilePathListModel(std::vector<base::FilePath> files,
                      std::vector<base::FilePath> directories);
    ~FilePathListModel() override;
    // ui::TableModel:
    int RowCount() override;
    base::string16 GetText(int row, int column_id) override;
    gfx::ImageSkia GetIcon(int row) override;
    base::string16 GetTooltip(int row) override;
    void SetObserver(ui::TableModelObserver*) override;

   private:
    const std::vector<base::FilePath> files_;
    const std::vector<base::FilePath> directories_;
    DISALLOW_COPY_AND_ASSIGN(FilePathListModel);
  };

  NativeFileSystemUsageBubbleView(views::View* anchor_view,
                                  content::WebContents* web_contents,
                                  const url::Origin& origin,
                                  Usage usage);
  ~NativeFileSystemUsageBubbleView() override;

  // LocationBarBubbleDelegateView:
  base::string16 GetAccessibleWindowTitle() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool ShouldShowCloseButton() const override;
  void Init() override;
  bool Cancel() override;
  bool Close() override;
  void WindowClosing() override;
  void CloseBubble() override;
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // Singleton instance of the bubble. The bubble can only be shown on the
  // active browser window, so there is no case in which it will be shown
  // twice at the same time.
  static NativeFileSystemUsageBubbleView* bubble_;

  const url::Origin origin_;
  const Usage usage_;
  FilePathListModel writable_paths_model_;
  FilePathListModel readable_paths_model_;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemUsageBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_VIEW_H_
