// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_shelf_context_menu.h"
#include "ui/base/ui_base_types.h"

class DownloadItemView;

namespace gfx {
class Rect;
}

namespace views {
class MenuRunner;
class Widget;
}

class DownloadBubbleUIController;

class DownloadShelfContextMenuView : public DownloadShelfContextMenu {
 public:
  // TODO(crbug.com/40756678): Remove dependency on DownloadItemView.
  explicit DownloadShelfContextMenuView(DownloadItemView* download_item_view);
  explicit DownloadShelfContextMenuView(
      base::WeakPtr<DownloadUIModel> download_ui_model);
  DownloadShelfContextMenuView(
      base::WeakPtr<DownloadUIModel> download_ui_model,
      base::WeakPtr<DownloadBubbleUIController> bubble_controller);
  DownloadShelfContextMenuView(const DownloadShelfContextMenuView&) = delete;
  DownloadShelfContextMenuView& operator=(const DownloadShelfContextMenuView&) =
      delete;
  ~DownloadShelfContextMenuView() override;

  base::TimeTicks close_time() const { return close_time_; }

  // |rect| is the bounding area for positioning the menu in screen coordinates.
  // The menu will be positioned above or below but not overlapping |rect|.
  void Run(views::Widget* parent_widget,
           const gfx::Rect& rect,
           ui::MenuSourceType source_type,
           base::RepeatingClosure on_menu_closed_callback);

  void SetOnMenuWillShowCallback(base::OnceClosure on_menu_will_show_callback);

 private:
  // Callback for MenuRunner.
  void OnMenuClosed(base::RepeatingClosure on_menu_closed_callback);
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;

  void ExecuteCommand(int command_id, int event_flags) override;

  // Parent download item view.
  // TODO(crbug.com/40756678): Remove dependency on DownloadItemView.
  raw_ptr<DownloadItemView> download_item_view_ = nullptr;

  // Use this instead of DownloadItemView to submit download for feedback.
  base::WeakPtr<DownloadBubbleUIController> bubble_controller_ = nullptr;

  base::OnceClosure on_menu_will_show_callback_;

  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Time the menu was closed.
  base::TimeTicks close_time_;

  // Determines whether we should record if a DownloadCommand was executed.
  bool download_commands_executed_recorded_[DownloadCommands::kMaxValue + 1] = {
      false};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_VIEW_H_
