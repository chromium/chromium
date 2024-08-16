// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/task_manager/task_manager_table_model.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/process/process_handle.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/ui/task_manager/task_manager_columns.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/common/buildflags.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/common/result_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/base/text/bytes_formatting.h"

namespace task_manager {

namespace {

const char kCpuTextFormatString[] = "%.1f";

#if BUILDFLAG(IS_MAC)
// Match Activity Monitor's default refresh rate.
const int64_t kRefreshTimeMS = 2000;
#else
const int64_t kRefreshTimeMS = 1000;
#endif  // BUILDFLAG(IS_MAC)

// The columns that are shared by a group will show the value of the column
// only once per group.
bool IsSharedByGroup(int column_id) {
  switch (column_id) {
    case IDS_TASK_MANAGER_MEM_FOOTPRINT_COLUMN:
    case IDS_TASK_MANAGER_SWAPPED_MEM_COLUMN:
    case IDS_TASK_MANAGER_CPU_COLUMN:
    case IDS_TASK_MANAGER_START_TIME_COLUMN:
    case IDS_TASK_MANAGER_CPU_TIME_COLUMN:
    case IDS_TASK_MANAGER_NET_COLUMN:
    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN:
    case IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN:
    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN:
    case IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN:
    case IDS_TASK_MANAGER_HARD_FAULTS_COLUMN:
    case IDS_TASK_MANAGER_OPEN_FD_COUNT_COLUMN:
    case IDS_TASK_MANAGER_PROCESS_PRIORITY_COLUMN:
      return true;
    default:
      return false;
  }
}

// Used to sort various column values.
template <class T>
int ValueCompare(T value1, T value2) {
  if (value1 == value2)
    return 0;
  return value1 < value2 ? -1 : 1;
}

// This forces NaN values to the bottom of the table.
template <>
int ValueCompare(double value1, double value2) {
  if (std::isnan(value1))
    return std::isnan(value2) ? 0 : -1;
  if (std::isnan(value2))
    return 1;
  if (value1 == value2)
    return 0;
  return value1 < value2 ? -1 : 1;
}

// This is used to sort process priority. We want the backgrounded process (with
// a true value) to come first.
int BooleanCompare(bool value1, bool value2) {
  if (value1 == value2)
    return 0;

  return value1 ? -1 : 1;
}

// Used when one or both of the results to compare are unavailable.
int OrderUnavailableValue(bool v1, bool v2) {
  if (!v1 && !v2)
    return 0;
  return v1 ? 1 : -1;
}

}  // namespace

// A class to stringify the task manager's values into string16s and to
// cache the common strings that will be reused many times like "N/A" and so on.
class TaskManagerValuesStringifier {
 public:
  TaskManagerValuesStringifier()
      : n_a_string_(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT)),
        zero_string_(u"0"),
        backgrounded_string_(
            l10n_util::GetStringUTF16(IDS_TASK_MANAGER_BACKGROUNDED_TEXT)),
        foregrounded_string_(
            l10n_util::GetStringUTF16(IDS_TASK_MANAGER_FOREGROUNDED_TEXT)),
        asterisk_string_(u"*"),
        unknown_string_(
            l10n_util::GetStringUTF16(IDS_TASK_MANAGER_UNKNOWN_VALUE_TEXT)),
        disabled_nacl_debugging_string_(l10n_util::GetStringUTF16(
            IDS_TASK_MANAGER_DISABLED_NACL_DBG_TEXT)) {}

  TaskManagerValuesStringifier(const TaskManagerValuesStringifier&) = delete;
  TaskManagerValuesStringifier& operator=(const TaskManagerValuesStringifier&) =
      delete;
  ~TaskManagerValuesStringifier() = default;

  std::u16string GetCpuUsageText(double cpu_usage) {
    if (std::isnan(cpu_usage))
      return n_a_string_;
    return base::UTF8ToUTF16(base::StringPrintf(kCpuTextFormatString,
                                                cpu_usage));
  }

  std::u16string GetStartTimeText(base::Time start_time) {
    if (start_time.is_null())
      return n_a_string_;

    return base::TimeFormatShortDateAndTime(start_time);
  }

  std::u16string GetCpuTimeText(base::TimeDelta cpu_time) {
    if (cpu_time.is_zero())
      return n_a_string_;

    std::u16string duration;
    return base::TimeDurationFormatWithSeconds(
               cpu_time, base::DURATION_WIDTH_NARROW, &duration)
               ? duration
               : n_a_string_;
  }

  std::u16string GetMemoryUsageText(int64_t memory_usage, bool has_duplicates) {
    if (memory_usage == -1)
      return n_a_string_;

#if BUILDFLAG(IS_MAC)
    // System expectation is to show "100 kB", "200 MB", etc.
    // TODO(thakis): [This TODO has been taken as is from the old task manager]:
    // Switch to metric units (as opposed to powers of two).
    std::u16string memory_text = ui::FormatBytes(memory_usage);
#else
    std::u16string memory_text = base::FormatNumber(memory_usage / 1024);
    // Adjust number string if necessary.
    base::i18n::AdjustStringForLocaleDirection(&memory_text);
    memory_text = l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_MEM_CELL_TEXT,
                                             memory_text);
#endif  // BUILDFLAG(IS_MAC)

    if (has_duplicates)
      memory_text += asterisk_string_;

    return memory_text;
  }

  std::u16string GetIdleWakeupsText(int idle_wakeups) {
    if (idle_wakeups == -1)
      return n_a_string_;

    return base::FormatNumber(idle_wakeups);
  }

  std::u16string GetHardFaultsText(int hard_faults) {
    if (hard_faults == -1)
      return n_a_string_;

    return base::FormatNumber(hard_faults);
  }

  std::u16string GetNaClPortText(int nacl_port) {
    // Only called if NaCl debug stub ports are enabled.

    if (nacl_port == nacl::kGdbDebugStubPortUnused)
      return n_a_string_;

    if (nacl_port == nacl::kGdbDebugStubPortUnknown)
      return unknown_string_;

    return base::NumberToString16(nacl_port);
  }

  std::u16string GetWindowsHandlesText(int64_t current, int64_t peak) {
    return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_HANDLES_CELL_TEXT,
                                      base::NumberToString16(current),
                                      base::NumberToString16(peak));
  }

  std::u16string GetNetworkUsageText(int64_t network_usage) {
    if (network_usage == -1)
      return n_a_string_;

    if (network_usage == 0)
      return zero_string_;

    std::u16string net_byte = ui::FormatSpeed(network_usage);
    // Force number string to have LTR directionality.
    return base::i18n::GetDisplayStringInLTRDirectionality(net_byte);
  }

  std::u16string GetProcessIdText(base::ProcessId proc_id) {
    return base::NumberToString16(proc_id);
  }

  std::u16string FormatAllocatedAndUsedMemory(int64_t allocated, int64_t used) {
    return l10n_util::GetStringFUTF16(
        IDS_TASK_MANAGER_CACHE_SIZE_CELL_TEXT,
        ui::FormatBytesWithUnits(allocated, ui::DATA_UNITS_KIBIBYTE, false),
        ui::FormatBytesWithUnits(used, ui::DATA_UNITS_KIBIBYTE, false));
  }

  std::u16string GetWebCacheStatText(
      const blink::WebCacheResourceTypeStat& stat) {
    return GetMemoryUsageText(stat.size, false);
  }

  std::u16string GetKeepaliveCountText(int keepalive_count) const {
    if (keepalive_count < 0)
      return n_a_string();
    return base::NumberToString16(keepalive_count);
  }

  const std::u16string& n_a_string() const { return n_a_string_; }
  const std::u16string& zero_string() const { return zero_string_; }
  const std::u16string& backgrounded_string() const {
    return backgrounded_string_;
  }
  const std::u16string& foregrounded_string() const {
    return foregrounded_string_;
  }
  const std::u16string& asterisk_string() const { return asterisk_string_; }
  const std::u16string& unknown_string() const { return unknown_string_; }
  const std::u16string& disabled_nacl_debugging_string() const {
    return disabled_nacl_debugging_string_;
  }

 private:
  // The localized string "N/A".
  const std::u16string n_a_string_;

  // The value 0 as a string "0".
  const std::u16string zero_string_;

  // The localized string "Backgrounded" for process priority.
  const std::u16string backgrounded_string_;

  // The localized string "Foregrounded" for process priority.
  const std::u16string foregrounded_string_;

  // The string "*" that is used to show that there exists duplicates in the
  // GPU memory.
  const std::u16string asterisk_string_;

  // The string "Unknown".
  const std::u16string unknown_string_;

  // The string to show on the NaCl debug port column cells when the flag
  // #enable-nacl-debug is disabled.
  const std::u16string disabled_nacl_debugging_string_;
};

////////////////////////////////////////////////////////////////////////////////
// TableSortDescriptor:
////////////////////////////////////////////////////////////////////////////////

TableSortDescriptor::TableSortDescriptor()
    : sorted_column_id(-1),
      is_ascending(false) {
}

TableSortDescriptor::TableSortDescriptor(int col_id, bool ascending)
    : sorted_column_id(col_id),
      is_ascending(ascending) {
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerTableModel:
////////////////////////////////////////////////////////////////////////////////

TaskManagerTableModel::TaskManagerTableModel(TableViewDelegate* delegate)
    : TaskManagerObserver(base::Milliseconds(kRefreshTimeMS),
                          REFRESH_TYPE_NONE),
      table_view_delegate_(delegate),
      table_model_observer_(nullptr),
      stringifier_(new TaskManagerValuesStringifier),
#if BUILDFLAG(ENABLE_NACL)
      is_nacl_debugging_flag_enabled_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableNaClDebug)) {
#else
      is_nacl_debugging_flag_enabled_(false) {
#endif  // BUILDFLAG(ENABLE_NACL)
  DCHECK(delegate);
  StartUpdating();
}

TaskManagerTableModel::~TaskManagerTableModel() {
  StopUpdating();
}

size_t TaskManagerTableModel::RowCount() {
  return tasks_.size();
}

std::u16string TaskManagerTableModel::GetText(size_t row, int column) {
  if (IsSharedByGroup(column) && !IsTaskFirstInGroup(row))
    return std::u16string();

  switch (column) {
    case IDS_TASK_MANAGER_TASK_COLUMN:
      return observed_task_manager()->GetTitle(tasks_[row]);

    case IDS_TASK_MANAGER_PROFILE_NAME_COLUMN:
      return observed_task_manager()->GetProfileName(tasks_[row]);

    case IDS_TASK_MANAGER_NET_COLUMN:
      return stringifier_->GetNetworkUsageText(
          observed_task_manager()->GetProcessTotalNetworkUsage(tasks_[row]));

    case IDS_TASK_MANAGER_CPU_COLUMN:
      return stringifier_->GetCpuUsageText(
          observed_task_manager()->GetPlatformIndependentCPUUsage(tasks_[row]));

    case IDS_TASK_MANAGER_CPU_TIME_COLUMN:
      return stringifier_->GetCpuTimeText(
          observed_task_manager()->GetCpuTime(tasks_[row]));

    case IDS_TASK_MANAGER_START_TIME_COLUMN:
      return stringifier_->GetStartTimeText(
          observed_task_manager()->GetStartTime(tasks_[row]));

    case IDS_TASK_MANAGER_MEM_FOOTPRINT_COLUMN:
      return stringifier_->GetMemoryUsageText(
          observed_task_manager()->GetMemoryFootprintUsage(tasks_[row]), false);

    case IDS_TASK_MANAGER_SWAPPED_MEM_COLUMN:
      return stringifier_->GetMemoryUsageText(
          observed_task_manager()->GetSwappedMemoryUsage(tasks_[row]), false);

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      if (observed_task_manager()->IsRunningInVM(tasks_[row])) {
        // Don't show the process ID if running inside a VM to avoid confusion
        // over conflicting pids.
        // TODO(b/122992194): Figure out if we need to change this to display
        // something for VM processes.
        return std::u16string();
      }
      return stringifier_->GetProcessIdText(
          observed_task_manager()->GetProcessId(tasks_[row]));

    case IDS_TASK_MANAGER_GDI_HANDLES_COLUMN: {
      int64_t current, peak;
      observed_task_manager()->GetGDIHandles(tasks_[row], &current, &peak);
      return stringifier_->GetWindowsHandlesText(current, peak);
    }

    case IDS_TASK_MANAGER_USER_HANDLES_COLUMN: {
      int64_t current, peak;
      observed_task_manager()->GetUSERHandles(tasks_[row], &current, &peak);
      return stringifier_->GetWindowsHandlesText(current, peak);
    }

    case IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN:
      return stringifier_->GetIdleWakeupsText(
          observed_task_manager()->GetIdleWakeupsPerSecond(tasks_[row]));

    case IDS_TASK_MANAGER_HARD_FAULTS_COLUMN:
      return stringifier_->GetHardFaultsText(
          observed_task_manager()->GetHardFaultsPerSecond(tasks_[row]));

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN: {
      blink::WebCacheResourceTypeStats stats;
      if (observed_task_manager()->GetWebCacheStats(tasks_[row], &stats))
        return stringifier_->GetWebCacheStatText(stats.images);
      return stringifier_->n_a_string();
    }

    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN: {
      blink::WebCacheResourceTypeStats stats;
      if (observed_task_manager()->GetWebCacheStats(tasks_[row], &stats))
        return stringifier_->GetWebCacheStatText(stats.scripts);
      return stringifier_->n_a_string();
    }

    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN: {
      blink::WebCacheResourceTypeStats stats;
      if (observed_task_manager()->GetWebCacheStats(tasks_[row], &stats))
        return stringifier_->GetWebCacheStatText(stats.css_style_sheets);
      return stringifier_->n_a_string();
    }

    case IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN: {
      bool has_duplicates = false;
      return stringifier_->GetMemoryUsageText(
          observed_task_manager()->GetGpuMemoryUsage(tasks_[row],
                                                     &has_duplicates),
          has_duplicates);
    }

    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
      return stringifier_->GetMemoryUsageText(
          observed_task_manager()->GetSqliteMemoryUsed(tasks_[row]), false);

    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN: {
      int64_t v8_allocated, v8_used;
      if (observed_task_manager()->GetV8Memory(tasks_[row],
                                               &v8_allocated,
                                               &v8_used)) {
        return stringifier_->FormatAllocatedAndUsedMemory(v8_allocated,
                                                          v8_used);
      }
      return stringifier_->n_a_string();
    }

    case IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN:
      if (!is_nacl_debugging_flag_enabled_)
        return stringifier_->disabled_nacl_debugging_string();

      return stringifier_->GetNaClPortText(
          observed_task_manager()->GetNaClDebugStubPort(tasks_[row]));

    case IDS_TASK_MANAGER_PROCESS_PRIORITY_COLUMN:
      return observed_task_manager()->IsTaskOnBackgroundedProcess(tasks_[row])
          ? stringifier_->backgrounded_string()
          : stringifier_->foregrounded_string();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
    case IDS_TASK_MANAGER_OPEN_FD_COUNT_COLUMN: {
      const int fd_count = observed_task_manager()->GetOpenFdCount(tasks_[row]);
      return fd_count >= 0 ? base::FormatNumber(fd_count)
                           : stringifier_->n_a_string();
    }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

    case IDS_TASK_MANAGER_KEEPALIVE_COUNT_COLUMN: {
      return stringifier_->GetKeepaliveCountText(
          observed_task_manager()->GetKeepaliveCount(tasks_[row]));
    }

    default:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

ui::ImageModel TaskManagerTableModel::GetIcon(size_t row) {
  return ui::ImageModel::FromImageSkia(
      observed_task_manager()->GetIcon(tasks_[row]));
}

void TaskManagerTableModel::SetObserver(
    ui::TableModelObserver* observer) {
  table_model_observer_ = observer;
}

int TaskManagerTableModel::CompareValues(size_t row1,
                                         size_t row2,
                                         int column_id) {
  switch (column_id) {
    case IDS_TASK_MANAGER_TASK_COLUMN:
    case IDS_TASK_MANAGER_PROFILE_NAME_COLUMN:
      return ui::TableModel::CompareValues(row1, row2, column_id);

    case IDS_TASK_MANAGER_NET_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetNetworkUsage(tasks_[row1]),
          observed_task_manager()->GetNetworkUsage(tasks_[row2]));

    case IDS_TASK_MANAGER_CPU_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetPlatformIndependentCPUUsage(tasks_[row1]),
          observed_task_manager()->GetPlatformIndependentCPUUsage(
              tasks_[row2]));

    case IDS_TASK_MANAGER_CPU_TIME_COLUMN:
      return ValueCompare(observed_task_manager()->GetCpuTime(tasks_[row1]),
                          observed_task_manager()->GetCpuTime(tasks_[row2]));

    case IDS_TASK_MANAGER_START_TIME_COLUMN:
      return ValueCompare(observed_task_manager()->GetStartTime(tasks_[row1]),
                          observed_task_manager()->GetStartTime(tasks_[row2]));

    case IDS_TASK_MANAGER_MEM_FOOTPRINT_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetMemoryFootprintUsage(tasks_[row1]),
          observed_task_manager()->GetMemoryFootprintUsage(tasks_[row2]));

    case IDS_TASK_MANAGER_SWAPPED_MEM_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetSwappedMemoryUsage(tasks_[row1]),
          observed_task_manager()->GetSwappedMemoryUsage(tasks_[row2]));

    case IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetNaClDebugStubPort(tasks_[row1]),
          observed_task_manager()->GetNaClDebugStubPort(tasks_[row2]));

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN: {
      bool vm1 = observed_task_manager()->IsRunningInVM(tasks_[row1]);
      bool vm2 = observed_task_manager()->IsRunningInVM(tasks_[row2]);
      if (vm1 != vm2) {
        return ValueCompare(vm1, vm2);
      }
      return ValueCompare(observed_task_manager()->GetProcessId(tasks_[row1]),
                          observed_task_manager()->GetProcessId(tasks_[row2]));
    }

    case IDS_TASK_MANAGER_GDI_HANDLES_COLUMN: {
      int64_t current1, peak1, current2, peak2;
      observed_task_manager()->GetGDIHandles(tasks_[row1], &current1, &peak1);
      observed_task_manager()->GetGDIHandles(tasks_[row2], &current2, &peak2);
      return ValueCompare(current1, current2);
    }

    case IDS_TASK_MANAGER_USER_HANDLES_COLUMN: {
      int64_t current1, peak1, current2, peak2;
      observed_task_manager()->GetUSERHandles(tasks_[row1], &current1, &peak1);
      observed_task_manager()->GetUSERHandles(tasks_[row2], &current2, &peak2);
      return ValueCompare(current1, current2);
    }

    case IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetIdleWakeupsPerSecond(tasks_[row1]),
          observed_task_manager()->GetIdleWakeupsPerSecond(tasks_[row2]));

    case IDS_TASK_MANAGER_HARD_FAULTS_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetHardFaultsPerSecond(tasks_[row1]),
          observed_task_manager()->GetHardFaultsPerSecond(tasks_[row2]));

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN: {
      blink::WebCacheResourceTypeStats stats1;
      blink::WebCacheResourceTypeStats stats2;
      bool row1_stats_valid =
          observed_task_manager()->GetWebCacheStats(tasks_[row1], &stats1);
      bool row2_stats_valid =
          observed_task_manager()->GetWebCacheStats(tasks_[row2], &stats2);
      if (!row1_stats_valid || !row2_stats_valid)
        return OrderUnavailableValue(row1_stats_valid, row2_stats_valid);

      switch (column_id) {
        case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
          return ValueCompare(stats1.images.size, stats2.images.size);
        case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
          return ValueCompare(stats1.scripts.size, stats2.scripts.size);
        case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
          return ValueCompare(stats1.css_style_sheets.size,
                              stats2.css_style_sheets.size);
        default:
          NOTREACHED_IN_MIGRATION();
          return 0;
      }
    }

    case IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN: {
      bool has_duplicates;
      return ValueCompare(
          observed_task_manager()->GetGpuMemoryUsage(tasks_[row1],
                                                     &has_duplicates),
          observed_task_manager()->GetGpuMemoryUsage(tasks_[row2],
                                                     &has_duplicates));
    }

    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN: {
      int64_t allocated1, allocated2, used1, used2;
      bool row1_valid = observed_task_manager()->GetV8Memory(tasks_[row1],
                                                             &allocated1,
                                                             &used1);
      bool row2_valid = observed_task_manager()->GetV8Memory(tasks_[row2],
                                                             &allocated2,
                                                             &used2);
      if (!row1_valid || !row2_valid)
        return OrderUnavailableValue(row1_valid, row2_valid);

      return ValueCompare(allocated1, allocated2);
    }

    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetSqliteMemoryUsed(tasks_[row1]),
          observed_task_manager()->GetSqliteMemoryUsed(tasks_[row2]));

    case IDS_TASK_MANAGER_PROCESS_PRIORITY_COLUMN: {
      const bool is_proc1_bg =
          observed_task_manager()->IsTaskOnBackgroundedProcess(tasks_[row1]);
      const bool is_proc2_bg =
          observed_task_manager()->IsTaskOnBackgroundedProcess(tasks_[row2]);
      return BooleanCompare(is_proc1_bg, is_proc2_bg);
    }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
    case IDS_TASK_MANAGER_OPEN_FD_COUNT_COLUMN: {
      const int proc1_fd_count =
          observed_task_manager()->GetOpenFdCount(tasks_[row1]);
      const int proc2_fd_count =
          observed_task_manager()->GetOpenFdCount(tasks_[row2]);
      return ValueCompare(proc1_fd_count, proc2_fd_count);
    }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
    case IDS_TASK_MANAGER_KEEPALIVE_COUNT_COLUMN: {
      return ValueCompare(
          observed_task_manager()->GetKeepaliveCount(tasks_[row1]),
          observed_task_manager()->GetKeepaliveCount(tasks_[row2]));
    }
    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

void TaskManagerTableModel::GetRowsGroupRange(size_t row_index,
                                              size_t* out_start,
                                              size_t* out_length) {
  size_t i = row_index;
  size_t limit = row_index + 1;
  if (!observed_task_manager()->IsRunningInVM(tasks_[row_index])) {
    const base::ProcessId process_id =
        observed_task_manager()->GetProcessId(tasks_[row_index]);
    while (i > 0 &&
           observed_task_manager()->GetProcessId(tasks_[i - 1]) == process_id &&
           !observed_task_manager()->IsRunningInVM(tasks_[i - 1])) {
      --i;
    }
    while (limit < RowCount() &&
           observed_task_manager()->GetProcessId(tasks_[limit]) == process_id &&
           !observed_task_manager()->IsRunningInVM(tasks_[limit])) {
      ++limit;
    }
  }
  *out_start = i;
  *out_length = limit - i;
}

void TaskManagerTableModel::OnTaskAdded(TaskId id) {
  // For the table view scrollbar to behave correctly we must inform it that
  // a new task has been added.

  // We will get a newly sorted list from the task manager as opposed to just
  // adding |id| to |tasks_| because we want to keep |tasks_| sorted by proc IDs
  // and then by Task IDs.
  tasks_ = observed_task_manager()->GetTaskIdsList();

  if (table_model_observer_) {
    std::vector<TaskId>::difference_type index =
        base::ranges::find(tasks_, id) - tasks_.begin();
    table_model_observer_->OnItemsAdded(index, 1);
  }
}

void TaskManagerTableModel::OnTaskToBeRemoved(TaskId id) {
  auto index = base::ranges::find(tasks_, id);
  if (index == tasks_.end())
    return;
  auto removed_index = static_cast<size_t>(index - tasks_.begin());
  tasks_.erase(index);
  if (table_model_observer_)
    table_model_observer_->OnItemsRemoved(removed_index, 1);
}

void TaskManagerTableModel::OnTasksRefreshed(
    const TaskIdList& task_ids) {
  tasks_ = task_ids;
  OnRefresh();
}

void TaskManagerTableModel::OnActiveTaskFetched(TaskId id) {
  if (!active_task_id_.has_value()) {
    active_task_id_ = id;
    table_view_delegate_->MaybeHighlightActiveTask();
  }
}

void TaskManagerTableModel::ActivateTask(size_t row_index) {
  observed_task_manager()->ActivateTask(tasks_[row_index]);
}

void TaskManagerTableModel::KillTask(size_t row_index) {
  observed_task_manager()->KillTask(tasks_[row_index]);
}

void TaskManagerTableModel::UpdateRefreshTypes(int column_id, bool visibility) {
  bool needs_refresh = visibility;
  RefreshType type = REFRESH_TYPE_NONE;
  switch (column_id) {
    case IDS_TASK_MANAGER_PROFILE_NAME_COLUMN:
    case IDS_TASK_MANAGER_TASK_COLUMN:
    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      return;  // The data in these columns do not change.

    case IDS_TASK_MANAGER_NET_COLUMN:
      type = REFRESH_TYPE_NETWORK_USAGE;
      break;

    case IDS_TASK_MANAGER_CPU_COLUMN:
      type = REFRESH_TYPE_CPU;
      break;

    case IDS_TASK_MANAGER_START_TIME_COLUMN:
      type = REFRESH_TYPE_START_TIME;
      break;

    case IDS_TASK_MANAGER_CPU_TIME_COLUMN:
      type = REFRESH_TYPE_CPU_TIME;
      break;

    case IDS_TASK_MANAGER_MEM_FOOTPRINT_COLUMN:
      type = REFRESH_TYPE_MEMORY_FOOTPRINT;
      break;

    case IDS_TASK_MANAGER_SWAPPED_MEM_COLUMN:
      type = REFRESH_TYPE_SWAPPED_MEM;
      if (table_view_delegate_->IsColumnVisible(
              IDS_TASK_MANAGER_SWAPPED_MEM_COLUMN)) {
        needs_refresh = true;
      }
      break;

    case IDS_TASK_MANAGER_GDI_HANDLES_COLUMN:
    case IDS_TASK_MANAGER_USER_HANDLES_COLUMN:
      type = REFRESH_TYPE_HANDLES;
      if (table_view_delegate_->IsColumnVisible(
              IDS_TASK_MANAGER_GDI_HANDLES_COLUMN) ||
          table_view_delegate_->IsColumnVisible(
              IDS_TASK_MANAGER_USER_HANDLES_COLUMN)) {
        needs_refresh = true;
      }
      break;

    case IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN:
      type = REFRESH_TYPE_IDLE_WAKEUPS;
      break;

    case IDS_TASK_MANAGER_HARD_FAULTS_COLUMN:
      type = REFRESH_TYPE_HARD_FAULTS;
      break;

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
      type = REFRESH_TYPE_WEBCACHE_STATS;
      if (table_view_delegate_->IsColumnVisible(
              IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN) ||
          table_view_delegate_->IsColumnVisible(
              IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN) ||
          table_view_delegate_->IsColumnVisible(
              IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN)) {
        needs_refresh = true;
      }
      break;

    case IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN:
      type = REFRESH_TYPE_GPU_MEMORY;
      break;

    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
      type = REFRESH_TYPE_SQLITE_MEMORY;
      break;

    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN:
      type = REFRESH_TYPE_V8_MEMORY;
      break;

    case IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN:
      type = REFRESH_TYPE_NACL;
      needs_refresh = needs_refresh && is_nacl_debugging_flag_enabled_;
      break;

    case IDS_TASK_MANAGER_PROCESS_PRIORITY_COLUMN:
      type = REFRESH_TYPE_PRIORITY;
      break;

    case IDS_TASK_MANAGER_KEEPALIVE_COUNT_COLUMN:
      type = REFRESH_TYPE_KEEPALIVE_COUNT;
      break;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
    case IDS_TASK_MANAGER_OPEN_FD_COUNT_COLUMN:
      type = REFRESH_TYPE_FD_COUNT;
      break;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  if (needs_refresh)
    AddRefreshType(type);
  else
    RemoveRefreshType(type);
}

bool TaskManagerTableModel::IsTaskKillable(size_t row_index) const {
  return observed_task_manager()->IsTaskKillable(tasks_[row_index]);
}

void TaskManagerTableModel::RetrieveSavedColumnsSettingsAndUpdateTable() {
  if (!g_browser_process->local_state())
    return;

  const base::Value::Dict& dictionary =
      g_browser_process->local_state()->GetDict(
          prefs::kTaskManagerColumnVisibility);

  // Do a best effort of retrieving the correct settings from the local state.
  // Use the default settings of the value if it fails to be retrieved.
  const std::string* sorted_col_id = dictionary.FindString(kSortColumnIdKey);
  bool sort_is_ascending =
      dictionary.FindBool(kSortIsAscendingKey).value_or(true);

  for (size_t i = 0; i < kColumnsSize; ++i) {
    const int col_id = kColumns[i].id;
    const std::string col_id_key(GetColumnIdAsString(col_id));

    if (col_id_key.empty())
      continue;

    bool col_visibility = dictionary.FindBoolByDottedPath(col_id_key)
                              .value_or(kColumns[i].default_visibility);

    // If the above FindBoolByDottedPath() fails, the |col_visibility| remains
    // at the default visibility.
    columns_settings_.SetByDottedPath(col_id_key, col_visibility);
    table_view_delegate_->SetColumnVisibility(col_id, col_visibility);
    UpdateRefreshTypes(col_id, col_visibility);

    if (col_visibility) {
      if (sorted_col_id && *sorted_col_id == col_id_key) {
        table_view_delegate_->SetSortDescriptor(
            TableSortDescriptor(col_id, sort_is_ascending));
      }
    }
  }
}

void TaskManagerTableModel::StoreColumnsSettings() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return;

  ScopedDictPrefUpdate dict_update(local_state,
                                   prefs::kTaskManagerColumnVisibility);

  for (const auto item : columns_settings_) {
    dict_update->SetByDottedPath(item.first, item.second.Clone());
  }

  // Store the current sort status to be restored again at startup.
  if (!table_view_delegate_->IsTableSorted()) {
    dict_update->Set(kSortColumnIdKey, "");
  } else {
    const auto& sort_descriptor = table_view_delegate_->GetSortDescriptor();
    dict_update->Set(kSortColumnIdKey,
                     GetColumnIdAsString(sort_descriptor.sorted_column_id));
    dict_update->Set(kSortIsAscendingKey, sort_descriptor.is_ascending);
  }
}

void TaskManagerTableModel::ToggleColumnVisibility(int column_id) {
  bool new_visibility = !table_view_delegate_->IsColumnVisible(column_id);
  if (table_view_delegate_->SetColumnVisibility(column_id, new_visibility)) {
    columns_settings_.SetByDottedPath(GetColumnIdAsString(column_id),
                                      new_visibility);
    UpdateRefreshTypes(column_id, new_visibility);
  }
}

std::optional<size_t> TaskManagerTableModel::GetRowForWebContents(
    content::WebContents* web_contents) {
  TaskId task_id =
      observed_task_manager()->GetTaskIdForWebContents(web_contents);
  auto index = base::ranges::find(tasks_, task_id);
  if (index == tasks_.end())
    return std::nullopt;
  return static_cast<size_t>(index - tasks_.begin());
}

std::optional<size_t> TaskManagerTableModel::GetRowForActiveTask() {
  if (!active_task_id_.has_value()) {
    return std::nullopt;
  }
  auto index = base::ranges::find(tasks_, active_task_id_.value());
  if (index == tasks_.end()) {
    return std::nullopt;
  }
  return static_cast<size_t>(index - tasks_.begin());
}

void TaskManagerTableModel::StartUpdating() {
  TaskManagerInterface::GetTaskManager()->AddObserver(this);
  tasks_ = observed_task_manager()->GetTaskIdsList();
  OnRefresh();

  // In order for the scrollbar of the TableView to work properly on startup of
  // the task manager, we must invoke TableModelObserver::OnModelChanged() which
  // in turn will invoke TableView::NumRowsChanged(). This will adjust the
  // vertical scrollbar correctly. crbug.com/570966.
  if (table_model_observer_)
    table_model_observer_->OnModelChanged();
}

void TaskManagerTableModel::StopUpdating() {
  observed_task_manager()->RemoveObserver(this);
}

void TaskManagerTableModel::OnRefresh() {
  if (table_model_observer_)
    table_model_observer_->OnItemsChanged(0, RowCount());
}

bool TaskManagerTableModel::IsTaskFirstInGroup(size_t row_index) const {
  if (row_index == 0)
    return true;

  if (observed_task_manager()->GetProcessId(tasks_[row_index - 1]) !=
      observed_task_manager()->GetProcessId(tasks_[row_index]))
    return true;

  if (observed_task_manager()->IsRunningInVM(tasks_[row_index - 1]) !=
      observed_task_manager()->IsRunningInVM(tasks_[row_index]))
    return true;

  return false;
}

}  // namespace task_manager
