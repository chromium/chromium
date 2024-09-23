// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_TAB_LIST_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_TAB_LIST_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"

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
class DesktopMediaTabList : public DesktopMediaListController::ListView {
  METADATA_HEADER(DesktopMediaTabList, DesktopMediaListController::ListView)

 public:
  DesktopMediaTabList(DesktopMediaListController* controller,
                      const std::u16string& accessible_name);
  DesktopMediaTabList(const DesktopMediaTabList&) = delete;
  DesktopMediaTabList& operator=(const DesktopMediaTabList&) = delete;
  ~DesktopMediaTabList() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;

  // DesktopMediaListController::ListView:
  std::optional<content::DesktopMediaID> GetSelection() override;
  DesktopMediaListController::SourceListListener* GetSourceListListener()
      override;
  void ClearSelection() override;

  // Called to indicate the preview image of the source indicated by index has
  // been updated.
  void OnPreviewUpdated(size_t index);

 private:
  std::unique_ptr<views::View> BuildUI(std::unique_ptr<views::TableView> table);
  void OnSelectionChanged();
  void ClearPreview();
  void ClearPreviewImageIfUnchanged(size_t previous_preview_set_count);

  // Helper for UMA-tracking of how often a user highlights a discarded tab.
  // (When that happens, the user will not see a tab preview.)
  void RecordSourceDiscardedStatus(const DesktopMediaList::Source& source);

  friend class DesktopMediaPickerViewsTestApi;
  friend class DesktopMediaTabListTest;

  raw_ptr<DesktopMediaListController, DanglingUntriaged> controller_;
  std::unique_ptr<TabListModel> model_;
  std::unique_ptr<TabListViewObserver> view_observer_;

  // These members are owned in the tree of views under this ListView's children
  // so it's safe to store raw pointers to them.
  raw_ptr<views::TableView> table_;
  raw_ptr<views::View> preview_wrapper_;
  raw_ptr<views::ImageView> preview_;
  raw_ptr<views::Label> empty_preview_label_;
  raw_ptr<views::Label> preview_label_;
  raw_ptr<views::ScrollView> scroll_view_;

  // Counts the number of times preview_ has been set to an image.
  size_t preview_set_count_ = 0;

  // For UMA purposes only, we track how often the user interacts with
  // the list and:
  // 1. Never ends up highlighting a discarded tab.
  // 2. Ends up highlighting a discarded tab at least once.
  //
  // For a good trade-off between complexity and accuracy, we discount
  // the possibility of tabs being discarded after they're highlighted.
  bool discarded_tab_highlighted_ = false;
  bool non_discarded_tab_highlighted_ = false;

  base::WeakPtrFactory<DesktopMediaTabList> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_TAB_LIST_H_
