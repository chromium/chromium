// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_COLUMNS_H_
#define CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_COLUMNS_H_

#include <stddef.h>

#include <array>
#include <string_view>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "components/nacl/common/buildflags.h"
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

// On Mac: Width of "a" and most other letters/digits in "small" table views.
inline constexpr int kCharWidth = 6;

// The task manager table columns and their properties.
//
// IMPORTANT: Do NOT change the below list without updating
// `GetColumnIdAsString()`, whose switch statement cannot be made
// exhaustive (given pure-integral inputs).
inline constexpr std::array kColumns = {
    TableColumnData{.id = IDS_TASK_MANAGER_TASK_COLUMN,
                    .align = ui::TableColumn::LEFT,
                    .width = -1,
                    .percent = 1,
                    .min_width = 120,
                    .max_width = 600,
                    .sortable = true,
                    .initial_sort_is_ascending = true,
                    .default_visibility = true},
    TableColumnData{.id = IDS_TASK_MANAGER_PROFILE_NAME_COLUMN,
                    .align = ui::TableColumn::LEFT,
                    .width = -1,
                    .percent = 0,
                    .min_width = 60,
                    .max_width = 200,
                    .sortable = true,
                    .initial_sort_is_ascending = true,
                    .default_visibility = false},
    TableColumnData{
        .id = IDS_TASK_MANAGER_MEM_FOOTPRINT_COLUMN,
        .align = ui::TableColumn::RIGHT,
        .width = -1,
        .percent = 0,
        .min_width = std::size("800 MiB") * kCharWidth,
        .max_width = std::size("Memory Footprint") * kCharWidth * 3 / 2,
        .sortable = true,
        .initial_sort_is_ascending = false,
        .default_visibility = true},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    TableColumnData{.id = IDS_TASK_MANAGER_SWAPPED_MEM_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("800 MiB") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = false,
                    .default_visibility = false},
#endif

// Make the CPU column min width a bit wider on macOS. When you click a column
// to make it the primary sort column a caret appears to the right of the
// column's label. Without a little extra space, the tableview squeezes the
// caret in by tail-truncating the label, which looks terrible.
#if BUILDFLAG(IS_MAC)
    TableColumnData{.id = IDS_TASK_MANAGER_CPU_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("0099.9") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = false,
                    .default_visibility = true},
#else
    TableColumnData{.id = IDS_TASK_MANAGER_CPU_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("99.9") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = false,
                    .default_visibility = true},
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
    TableColumnData{.id = IDS_TASK_MANAGER_CPU_TIME_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("1234h 42m 30s") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = false,
                    .default_visibility = false},
    TableColumnData{.id = IDS_TASK_MANAGER_START_TIME_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("12/13/14 11:44:30 PM") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = true,
                    .default_visibility = false},
#endif
    TableColumnData{.id = IDS_TASK_MANAGER_NET_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("150 kiB/s") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = false,
                    .default_visibility = true},
    TableColumnData{.id = IDS_TASK_MANAGER_PROCESS_ID_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("73099  ") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = true,
                    .default_visibility = true},

#if BUILDFLAG(IS_WIN)
    TableColumnData{.id = IDS_TASK_MANAGER_GDI_HANDLES_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = 0,
                    .max_width = 0,
                    .sortable = true,
                    .initial_sort_is_ascending = false,
                    .default_visibility = false},
    TableColumnData{.id = IDS_TASK_MANAGER_USER_HANDLES_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = 0,
                    .max_width = 0,
                    .sortable = true,
                    .initial_sort_is_ascending = false,
                    .default_visibility = false},
#endif

    TableColumnData{
        .id = IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN,
        .align = ui::TableColumn::RIGHT,
        .width = -1,
        .percent = 0,
        .min_width = std::size("2000.0K (2000.0 live)") * kCharWidth,
        .max_width = -1,
        .sortable = true,
        .initial_sort_is_ascending = false,
        .default_visibility = false},
    TableColumnData{
        .id = IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN,
        .align = ui::TableColumn::RIGHT,
        .width = -1,
        .percent = 0,
        .min_width = std::size("2000.0K (2000.0 live)") * kCharWidth,
        .max_width = -1,
        .sortable = true,
        .initial_sort_is_ascending = false,
        .default_visibility = false},
    TableColumnData{
        .id = IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN,
        .align = ui::TableColumn::RIGHT,
        .width = -1,
        .percent = 0,
        .min_width = std::size("2000.0K (2000.0 live)") * kCharWidth,
        .max_width = -1,
        .sortable = true,
        .initial_sort_is_ascending = false,
        .default_visibility = false},
    TableColumnData{.id = IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("2000.0K") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = false,
                    .default_visibility = false},
    TableColumnData{.id = IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("800 kB") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = false,
                    .default_visibility = false},

#if BUILDFLAG(ENABLE_NACL)
    TableColumnData{.id = IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("32767") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = true,
                    .default_visibility = false},
#endif  // BUILDFLAG(ENABLE_NACL)

    TableColumnData{
        .id = IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN,
        .align = ui::TableColumn::RIGHT,
        .width = -1,
        .percent = 0,
        .min_width = std::size("2000.0K (2000.0 live)") * kCharWidth,
        .max_width = -1,
        .sortable = true,
        .initial_sort_is_ascending = false,
        .default_visibility = false},
    TableColumnData{.id = IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("idlewakeups") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = false,
                    .default_visibility = false},

#if BUILDFLAG(IS_WIN)
    TableColumnData{.id = IDS_TASK_MANAGER_HARD_FAULTS_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("100000") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = false,
                    .default_visibility = false},
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
    TableColumnData{.id = IDS_TASK_MANAGER_OPEN_FD_COUNT_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("999") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = false,
                    .default_visibility = false},
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
    TableColumnData{.id = IDS_TASK_MANAGER_PROCESS_PRIORITY_COLUMN,
                    .align = ui::TableColumn::LEFT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("background") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = true,
                    .default_visibility = false},
    TableColumnData{.id = IDS_TASK_MANAGER_KEEPALIVE_COUNT_COLUMN,
                    .align = ui::TableColumn::RIGHT,
                    .width = -1,
                    .percent = 0,
                    .min_width = std::size("999") * kCharWidth,
                    .max_width = -1,
                    .sortable = true,
                    .initial_sort_is_ascending = false,
                    .default_visibility = false},
};
inline constexpr size_t kColumnsSize = std::size(kColumns);

// Session Restore Keys.
extern const char kSortColumnIdKey[];
extern const char kSortIsAscendingKey[];

// Returns the |column_id| as a string value to be used as keys in the user
// preferences.
std::string_view GetColumnIdAsString(int column_id);

}  // namespace task_manager

#endif  // CHROME_BROWSER_UI_TASK_MANAGER_TASK_MANAGER_COLUMNS_H_
