// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_TEST_API_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "content/public/browser/desktop_media_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class DesktopMediaPickerViews;
class DesktopMediaListController;

namespace ui {
class KeyEvent;
}  // namespace ui

namespace views {
class MdTextButton;
class TableView;
class View;
}  // namespace views

class DesktopMediaPickerViewsTestApi {
 public:
  DesktopMediaPickerViewsTestApi();
  DesktopMediaPickerViewsTestApi(const DesktopMediaPickerViewsTestApi&) =
      delete;
  DesktopMediaPickerViewsTestApi operator=(
      const DesktopMediaPickerViewsTestApi&) = delete;
  ~DesktopMediaPickerViewsTestApi();

  void set_picker(DesktopMediaPickerViews* picker) { picker_ = picker; }

  bool AudioSupported(DesktopMediaList::Type type) const;

  void FocusAudioShareControl();
  void PressMouseOnSourceAtIndex(size_t index, bool double_click = false);
  void PressKeyOnSourceAtIndex(size_t index, const ui::KeyEvent& event);
  void SelectTabForSourceType(DesktopMediaList::Type source_type);
  bool HasAudioShareControl() const;
  void SetAudioSharingApprovedByUser(bool allow);
  bool IsAudioSharingApprovedByUser() const;
  views::MdTextButton* GetReselectButton();

  bool HasSourceAtIndex(size_t index) const;
  void FocusSourceAtIndex(size_t index, bool select = true);
  void DoubleTapSourceAtIndex(size_t index);
  DesktopMediaList::Type GetSelectedSourceListType() const;
  absl::optional<int> GetSelectedSourceId() const;
  views::View* GetSelectedListView();
  DesktopMediaListController* GetSelectedController();

 private:
  const views::View* GetSourceAtIndex(size_t index) const;
  views::View* GetSourceAtIndex(size_t index);
  const views::TableView* GetTableView() const;
  views::TableView* GetTableView();

  raw_ptr<DesktopMediaPickerViews> picker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_TEST_API_H_
