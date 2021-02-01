// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_TEST_API_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_TEST_API_H_

#include "content/public/browser/desktop_media_id.h"

class DesktopMediaPickerViews;

namespace ui {
class KeyEvent;
}  // namespace ui

namespace views {
class Checkbox;
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

  void FocusAudioCheckbox();
  void PressMouseOnSourceAtIndex(size_t index, bool double_click = false);
  void PressKeyOnSourceAtIndex(size_t index, const ui::KeyEvent& event);
  void SelectTabForSourceType(content::DesktopMediaID::Type source_type);
  views::Checkbox* GetAudioShareCheckbox();

  bool HasSourceAtIndex(size_t index) const;
  void FocusSourceAtIndex(size_t index, bool select = true);
  void DoubleTapSourceAtIndex(size_t index);
  base::Optional<int> GetSelectedSourceId() const;
  views::View* GetSelectedListView();

 private:
  const views::View* GetSourceAtIndex(size_t index) const;
  views::View* GetSourceAtIndex(size_t index);
  const views::TableView* GetTableView() const;
  views::TableView* GetTableView();

  DesktopMediaPickerViews* picker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_TEST_API_H_
