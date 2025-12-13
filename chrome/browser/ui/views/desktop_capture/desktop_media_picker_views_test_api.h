// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_TEST_API_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_TEST_API_H_

#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "content/public/browser/desktop_media_id.h"

class DesktopMediaPaneView;
class DesktopMediaPickerImpl;
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

  void set_picker(DesktopMediaPickerImpl* picker) { picker_ = picker; }

  bool AudioSupported(DesktopMediaList::Type type) const;

  void FocusAudioShareControl();
  void PressMouseOnSourceAtIndex(size_t index, bool double_click = false);
  void PressKeyOnSourceAtIndex(size_t index, const ui::KeyEvent& event);
  void SelectTabForSourceType(DesktopMediaList::Type source_type);
  bool HasAudioShareControl() const;
  std::u16string_view GetAudioLabelText() const;
  void SetAudioSharingApprovedByUser(bool allow);
  bool IsAudioSharingApprovedByUser() const;
  bool IsAudioSharingControlEnabled() const;
  bool IsScreenAudioOffered() const;
  bool IsWindowAudioOffered() const;
  content::DesktopMediaID::AudioType GetWindowAudioType() const;
  views::MdTextButton* GetReselectButton();

  bool HasSourceAtIndex(size_t index) const;
  void FocusSourceAtIndex(size_t index, bool select = true);
  void DoubleTapSourceAtIndex(size_t index);
  DesktopMediaList::Type GetSelectedSourceListType() const;
  std::optional<int> GetSelectedSourceId() const;
  views::View* GetSelectedListView();
  DesktopMediaListController* GetSelectedController();

  const DesktopMediaPaneView* GetActivePane() const;
  DesktopMediaPaneView* GetActivePane();

#if BUILDFLAG(IS_MAC)
  void OnPermissionUpdate(bool has_permission);
#endif

 private:
  const views::View* GetSourceAtIndex(size_t index) const;
  views::View* GetSourceAtIndex(size_t index);
  const views::TableView* GetTableView() const;
  views::TableView* GetTableView();

  raw_ptr<DesktopMediaPickerImpl> picker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_TEST_API_H_
