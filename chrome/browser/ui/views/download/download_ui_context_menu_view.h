// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_UI_CONTEXT_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_UI_CONTEXT_MENU_VIEW_H_

#include <array>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_ui_context_menu.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"

namespace gfx {
class Rect;
}

namespace views {
class MenuRunner;
class Widget;
}  // namespace views

class DownloadBubbleUIController;

class DownloadUiContextMenuView : public DownloadUiContextMenu {
 public:
  explicit DownloadUiContextMenuView(
      base::WeakPtr<DownloadUIModel> download_ui_model);
  DownloadUiContextMenuView(
      base::WeakPtr<DownloadUIModel> download_ui_model,
      base::WeakPtr<DownloadBubbleUIController> bubble_controller);
  DownloadUiContextMenuView(const DownloadUiContextMenuView&) = delete;
  DownloadUiContextMenuView& operator=(const DownloadUiContextMenuView&) =
      delete;
  ~DownloadUiContextMenuView() override;

  base::TimeTicks close_time() const { return close_time_; }

  // |rect| is the bounding area for positioning the menu in screen coordinates.
  // The menu will be positioned above or below but not overlapping |rect|.
  void Run(views::Widget* parent_widget,
           const gfx::Rect& rect,
           ui::mojom::MenuSourceType source_type,
           base::RepeatingClosure on_menu_closed_callback);

  void SetOnMenuWillShowCallback(base::OnceClosure on_menu_will_show_callback);

 private:
  // Callback for MenuRunner.
  void OnMenuClosed(base::RepeatingClosure on_menu_closed_callback);
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;

  void ExecuteCommand(int command_id, int event_flags) override;

  base::WeakPtr<DownloadBubbleUIController> bubble_controller_ = nullptr;

  base::OnceClosure on_menu_will_show_callback_;

  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Time the menu was closed.
  base::TimeTicks close_time_;

  // Determines whether we should record if a DownloadCommand was executed.
  std::array<bool, DownloadCommands::kMaxValue + 1>
      download_commands_executed_recorded_{false};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_UI_CONTEXT_MENU_VIEW_H_
