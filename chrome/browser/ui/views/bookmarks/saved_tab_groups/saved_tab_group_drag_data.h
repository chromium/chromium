// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_DRAG_DATA_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_DRAG_DATA_H_

#include <optional>

#include "base/uuid.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {
class OSExchangeData;
class ThemeProvider;
}  // namespace ui

namespace tab_groups {

class SavedTabGroupButton;

class SavedTabGroupDragData {
 public:
  explicit SavedTabGroupDragData(const base::Uuid guid);

  static const ui::ClipboardFormatType& GetFormatType();

  static std::optional<SavedTabGroupDragData> ReadFromOSExchangeData(
      const ui::OSExchangeData* data);

  static void WriteToOSExchangeData(SavedTabGroupButton* button,
                                    const gfx::Point& press_pt,
                                    const ui::ThemeProvider* theme_provider,
                                    ui::OSExchangeData* data);

  const std::optional<size_t>& insertion_index() { return insertion_index_; }
  void SetInsertionIndex(std::optional<size_t> insertion_index) {
    insertion_index_ = insertion_index;
  }

  const std::optional<gfx::Point>& location() { return location_; }
  void SetLocation(std::optional<gfx::Point> new_location) {
    location_ = new_location;
  }

  const base::Uuid guid() { return guid_; }

 private:
  // Insertion index if the drop is finished
  std::optional<size_t> insertion_index_;

  // Local coordinates of the drag.
  std::optional<gfx::Point> location_;

  // A copy of the group being dragged.
  const base::Uuid guid_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_DRAG_DATA_H_
