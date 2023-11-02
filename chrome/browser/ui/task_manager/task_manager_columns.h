// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_COLUMNS_H_
#define CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_COLUMNS_H_

#include <stddef.h>

#include "ui/base/models/table_model.h"

namespace task_manager {

// A collection of data to be used in the construction of a task manager table
// column.
struct TableColumnData {
  // The generated ID of the column. These can change from one build to another.
  // Their values are controlled by the generation from generated_resources.grd.
  int id;

  // The alignment of the text displayed in this column.
  ui::TableColumn::Alignment align;

  // |width| and |percent| used to define the size of the column. See
  // ui::TableColumn::width and ui::TableColumn::percent for details.
  int width;
  float percent;

  // min and max widths used for Mac's implementation and are ignored on Views.
  // If |max_width| is -1, a value of 1.5 * |min_width| will be used.
  int min_width;
  int max_width;

  // Is the column sortable.
  bool sortable;

  // Is the initial sort order ascending?
  bool initial_sort_is_ascending;

  // The default visibility of this column at startup of the table if no
  // visibility is stored for it in the prefs.
  bool default_visibility;
};

// The task manager table columns and their properties.
extern const TableColumnData kColumns[];
extern const size_t kColumnsSize;

// Session Restore Keys.
extern const char kSortColumnIdKey[];
extern const char kSortIsAscendingKey[];

// Returns the |column_id| as a string value to be used as keys in the user
// preferences.
std::string GetColumnIdAsString(int column_id);

}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_COLUMNS_H_
