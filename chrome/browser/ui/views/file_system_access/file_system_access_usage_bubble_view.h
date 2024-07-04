// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_VIEW_H_

#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/table_model.h"
#include "url/origin.h"

class FileSystemAccessUsageBubbleView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(FileSystemAccessUsageBubbleView,
                  LocationBarBubbleDelegateView)

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

  FileSystemAccessUsageBubbleView(const FileSystemAccessUsageBubbleView&) =
      delete;
  FileSystemAccessUsageBubbleView& operator=(
      const FileSystemAccessUsageBubbleView&) = delete;

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
    FilePathListModel(std::vector<base::FilePath> files,
                      std::vector<base::FilePath> directories);
    FilePathListModel(const FilePathListModel&) = delete;
    FilePathListModel& operator=(const FilePathListModel&) = delete;
    ~FilePathListModel() override;
    // ui::TableModel:
    size_t RowCount() override;
    std::u16string GetText(size_t row, int column_id) override;
    ui::ImageModel GetIcon(size_t row) override;
    std::u16string GetTooltip(size_t row) override;
    void SetObserver(ui::TableModelObserver*) override;

   private:
    const std::vector<base::FilePath> files_;
    const std::vector<base::FilePath> directories_;
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

  void OnDialogCancelled();

  // Singleton instance of the bubble. The bubble can only be shown on the
  // active browser window, so there is no case in which it will be shown
  // twice at the same time.
  static FileSystemAccessUsageBubbleView* bubble_;

  raw_ptr<views::View> readable_collapsible_list_view_;
  raw_ptr<views::View> writable_collapsible_list_view_;
  const url::Origin origin_;
  const Usage usage_;
  FilePathListModel readable_paths_model_;
  FilePathListModel writable_paths_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_VIEW_H_
