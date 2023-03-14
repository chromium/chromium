// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_DRAG_DATA_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_DRAG_DATA_H_

#include "base/guid.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {
class OSExchangeData;
class ThemeProvider;
}  // namespace ui

class SavedTabGroupButton;

class SavedTabGroupDragData {
 public:
  explicit SavedTabGroupDragData(const base::GUID guid);

  static const ui::ClipboardFormatType& GetFormatType();

  static absl::optional<SavedTabGroupDragData> ReadFromOSExchangeData(
      const ui::OSExchangeData* data);

  static void WriteToOSExchangeData(SavedTabGroupButton* button,
                                    const gfx::Point& press_pt,
                                    const ui::ThemeProvider* theme_provider,
                                    ui::OSExchangeData* data);

  const absl::optional<size_t>& insertion_index() { return insertion_index_; }
  void SetInsertionIndex(absl::optional<size_t> insertion_index) {
    insertion_index_ = insertion_index;
  }

  const absl::optional<gfx::Point>& location() { return location_; }
  void SetLocation(absl::optional<gfx::Point> new_location) {
    location_ = new_location;
  }

  const base::GUID guid() { return guid_; }

 private:
  // Insertion index if the drop is finished
  absl::optional<size_t> insertion_index_;

  // Local coordinates of the drag.
  absl::optional<gfx::Point> location_;

  // A copy of the group being dragged.
  const base::GUID guid_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_BAR_H_
