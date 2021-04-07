// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_VIEW_H_

#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/models/table_model.h"
#include "url/origin.h"

class FileSystemAccessUsageBubbleView : public LocationBarBubbleDelegateView {
 public:
  struct Usage {
    Usage();
    ~Usage();
    Usage(Usage&&);
    Usage& operator=(Usage&&);

    std::vector<base::FilePath> readable_files;
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
  static FileSystemAccessUsageBubbleView* GetBubble();

 private:
  class FilePathListModel : public ui::TableModel {
   public:
    FilePathListModel(const views::View* view,
                      std::vector<base::FilePath> files,
                      std::vector<base::FilePath> directories);
    ~FilePathListModel() override;
    // ui::TableModel:
    int RowCount() override;
    std::u16string GetText(int row, int column_id) override;
    gfx::ImageSkia GetIcon(int row) override;
    std::u16string GetTooltip(int row) override;
    void SetObserver(ui::TableModelObserver*) override;

   private:
    // The model needs access to the view it is in to access the correct theme
    // for icon colors.
    const views::View* const owner_;

    const std::vector<base::FilePath> files_;
    const std::vector<base::FilePath> directories_;
    DISALLOW_COPY_AND_ASSIGN(FilePathListModel);
  };

  FileSystemAccessUsageBubbleView(views::View* anchor_view,
                                  content::WebContents* web_contents,
                                  const url::Origin& origin,
                                  Usage usage);
  ~FileSystemAccessUsageBubbleView() override;

  // LocationBarBubbleDelegateView:
  std::u16string GetAccessibleWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  void Init() override;
  void WindowClosing() override;
  void CloseBubble() override;
  void ChildPreferredSizeChanged(views::View* child) override;

  void OnDialogCancelled();

  // Singleton instance of the bubble. The bubble can only be shown on the
  // active browser window, so there is no case in which it will be shown
  // twice at the same time.
  static FileSystemAccessUsageBubbleView* bubble_;

  const url::Origin origin_;
  const Usage usage_;
  FilePathListModel readable_paths_model_;
  FilePathListModel writable_paths_model_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemAccessUsageBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_VIEW_H_
