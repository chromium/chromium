// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/task_manager/task_manager_columns.h"

#include <string_view>

#include "base/notreached.h"
#include "chrome/grit/generated_resources.h"

namespace task_manager {

const char kSortColumnIdKey[] = "sort_column_id";
const char kSortIsAscendingKey[] = "sort_is_ascending";

// We can't derive session restore keys from the integer IDs of the columns
// since the IDs are generated, and so may change from one build to another.
// Instead we stringify the column ID symbol (i.e. for the ID
// IDS_TASK_MANAGER_TASK_COLUMN, we use the literal string
// "IDS_TASK_MANAGER_TASK_COLUMN").

#if defined(COLUMN_CASE)
#error Surprising name collision ┐(´∇｀)┌
#endif
#define COLUMN_CASE(column_id) \
  case column_id:              \
    return #column_id

std::string_view GetColumnIdAsString(int column_id) {
  switch (column_id) {
    COLUMN_CASE(IDS_TASK_MANAGER_TASK_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_PROFILE_NAME_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_MEM_FOOTPRINT_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_SWAPPED_MEM_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_CPU_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_START_TIME_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_CPU_TIME_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_NET_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_PROCESS_ID_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_GDI_HANDLES_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_USER_HANDLES_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_HARD_FAULTS_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_OPEN_FD_COUNT_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_PROCESS_PRIORITY_COLUMN);
    COLUMN_CASE(IDS_TASK_MANAGER_KEEPALIVE_COUNT_COLUMN);
  }
  NOTREACHED();
}

#undef COLUMN_CASE

}  // namespace task_manager
