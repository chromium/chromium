// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_TAB_LIST_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_TAB_LIST_H_

#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace views {
class TableView;
class View;
}  // namespace views

namespace {
class TabListModel;
class TabListViewObserver;
}  // namespace

// This class is one of the possible ListViews a DesktopMediaListController can
// control. It displays a table of sources, one per line, with their "thumbnail"
// scaled down and displayed as an icon on that line of the table. It is used to
// display a list of tabs which are possible cast sources.
//
// Internally, this class has two helper classes:
// * TabListModel, which is a ui::TableModel that proxies for DesktopMediaList -
//   it fetches data from the DesktopMediaList to populate the model, and
//   observes the DesktopMediaList to update the TableModel.
// * TabListViewObserver, which is a TableViewObserver that notifies the
//   controller when the user takes an action on the TableView.
//
// Since TableView really wants to be the child of a ScrollView, this class's
// internal view hierarchy actually looks like:
//   DesktopMediaTabList
//     ScrollView
//       [ScrollView internal helper Views]
//         TableView
class DesktopMediaTabList : public DesktopMediaListController::ListView {
 public:
  METADATA_HEADER(DesktopMediaTabList);
  DesktopMediaTabList(DesktopMediaListController* controller,
                      const std::u16string& accessible_name);
  DesktopMediaTabList(const DesktopMediaTabList&) = delete;
  DesktopMediaTabList& operator=(const DesktopMediaTabList&) = delete;
  ~DesktopMediaTabList() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;

  // DesktopMediaListController::ListView:
  base::Optional<content::DesktopMediaID> GetSelection() override;
  DesktopMediaListController::SourceListListener* GetSourceListListener()
      override;

 private:
  friend class DesktopMediaPickerViewsTestApi;

  DesktopMediaListController* controller_;
  std::unique_ptr<TabListModel> model_;
  std::unique_ptr<TabListViewObserver> view_observer_;
  views::TableView* child_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_TAB_LIST_H_
