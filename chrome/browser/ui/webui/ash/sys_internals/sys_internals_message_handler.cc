// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/sys_internals/sys_internals_message_handler.h"

#include <inttypes.h>

#include <cstdio>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/byte_count.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {

struct CpuInfo {
  int kernel;
  int user;
  int idle;
  int total;
};

struct GpuInfo {
  base::TimeDelta busy_time;
};

struct NpuInfo {
  base::TimeDelta busy_time;
};

// When counter overflow, it will restart from zero. base::Value do not
// supports 64-bit integer, and passing the counter as a double it may cause
// problems. Therefore, only use the last 31 bits of the counter and pass it
// as a 32-bit signed integer.
constexpr uint32_t COUNTER_MAX = 0x7FFFFFFFu;

template <typename T>
inline int ToCounter(T value) {
  DCHECK_GE(value, T(0));
  return static_cast<int>(value & COUNTER_MAX);
}

bool ParseProcStatLine(const std::string& line, std::vector<CpuInfo>* infos) {
  DCHECK(infos);
  uint64_t user = 0;
  uint64_t nice = 0;
  uint64_t sys = 0;
  uint64_t idle = 0;
  uint32_t cpu_index = 0;
  int vals = UNSAFE_TODO(sscanf(line.c_str(),
                                "cpu%" SCNu32 " %" SCNu64 " %" SCNu64
                                " %" SCNu64 " %" SCNu64,
                                &cpu_index, &user, &nice, &sys, &idle));
  if (vals != 5 || cpu_index >= infos->size()) {
    NOTREACHED();
  }

  CpuInfo& cpu_info = (*infos)[cpu_index];
  cpu_info.kernel = ToCounter(sys);
  cpu_info.user = ToCounter(user + nice);
  cpu_info.idle = ToCounter(idle);
  cpu_info.total = ToCounter(sys + user + nice + idle);

  return true;
}

bool GetCpuInfo(std::vector<CpuInfo>* infos) {
  DCHECK(infos);

  // WARNING: this method may return incomplete data because some processors may
  // be brought offline at runtime. /proc/stat does not report statistics of
  // offline processors. CPU usages of offline processors will be filled with
  // zeros.
  //
  // An example of output of /proc/stat when processor 0 and 3 are online, but
  // processor 1 and 2 are offline:
  //
  //   cpu  145292 20018 83444 1485410 995 44 3578 0 0 0
  //   cpu0 138060 19947 78350 1479514 570 44 3576 0 0 0
  //   cpu3 2033 32 1075 1400 52 0 1 0 0 0
  const char kProcStat[] = "/proc/stat";
  std::string contents;
  if (!base::ReadFileToString(base::FilePath(kProcStat), &contents)) {
    return false;
  }

  std::istringstream iss(contents);
  std::string line;

  // Skip the first line because it is just an aggregated number of
  // all cpuN lines.
  std::getline(iss, line);
  while (std::getline(iss, line)) {
    if (line.compare(0, 3, "cpu") != 0) {
      continue;
    }
    if (!ParseProcStatLine(line, infos)) {
      return false;
    }
  }

  return true;
}

void SetConstValue(base::Value::Dict* result) {
  DCHECK(result);
  int counter_max = static_cast<int>(COUNTER_MAX);
  result->SetByDottedPath("const.counterMax", counter_max);
}

void SetCpusValue(const std::vector<CpuInfo>& infos,
                  base::Value::Dict* result) {
  DCHECK(result);
  base::Value::List cpu_results;
  for (const CpuInfo& cpu : infos) {
    base::Value::Dict cpu_result;
    cpu_result.Set("user", cpu.user);
    cpu_result.Set("kernel", cpu.kernel);
    cpu_result.Set("idle", cpu.idle);
    cpu_result.Set("total", cpu.total);
    cpu_results.Append(std::move(cpu_result));
  }
  result->Set("cpus", std::move(cpu_results));
}

double GetAvailablePhysicalMemory(const base::SystemMemoryInfo& info) {
  base::ByteCount available =
      info.available.is_zero() ? info.free + info.reclaimable : info.available;

  return available.InBytesF();
}

void SetMemValue(const base::SystemMemoryInfo& info,
                 const base::VmStatInfo& vmstat,
                 base::Value::Dict* result) {
  DCHECK(result);
  base::Value::Dict mem_result;

  // For values that may exceed the range of 32-bit signed integer, use double.
  double total = info.total.InBytesF();
  mem_result.Set("total", total);
  mem_result.Set("available", GetAvailablePhysicalMemory(info));
  double swap_total = info.swap_total.InBytesF();
  mem_result.Set("swapTotal", swap_total);
  double swap_free = info.swap_free.InBytesF();
  mem_result.Set("swapFree", swap_free);

  mem_result.Set("pswpin", ToCounter(vmstat.pswpin));
  mem_result.Set("pswpout", ToCounter(vmstat.pswpout));

  result->Set("memory", std::move(mem_result));
}

void SetZramValue(const base::SwapInfo& info, base::Value::Dict* result) {
  DCHECK(result);
  base::Value::Dict zram_result;

  zram_result.Set("numReads", ToCounter(info.num_reads));
  zram_result.Set("numWrites", ToCounter(info.num_writes));

  // For values that may exceed the range of 32-bit signed integer, use double.
  zram_result.Set("comprDataSize", static_cast<double>(info.compr_data_size));
  zram_result.Set("origDataSize", static_cast<double>(info.orig_data_size));
  zram_result.Set("memUsedTotal", static_cast<double>(info.mem_used_total));

  result->Set("zram", std::move(zram_result));
}

constexpr char kI915EngineInfoPath[] = "/run/debugfs_gpu/i915_engine_info";

std::optional<GpuInfo> GetGpuInfoFromI915EngineInfo() {
  // Cumulative runtime information has been available on ChromeOS since Kernel
  // v5.10. In earlier versions, the engine info file may exist, but it will not
  // include the "Runtime:" stat.
  //
  // The implementation can be found in intel_engine_dump() at
  // drivers/gpu/drm/i915/gt/intel_engine_cs.c.
  std::string content;
  if (!base::ReadFileToStringNonBlocking(base::FilePath(kI915EngineInfoPath),
                                         &content)) {
    return std::nullopt;
  }

  base::TimeDelta busy_time;
  bool found_runtime = false;
  for (auto line : base::SplitStringPiece(content, "\n", base::KEEP_WHITESPACE,
                                          base::SPLIT_WANT_NONEMPTY)) {
    if (!line.starts_with("\tRuntime: ")) {
      continue;
    }

    // Multiple engines can run concurrently, causing the busy time to increase
    // faster than wall-clock time. For simplicity, we aggregate them and clamp
    // the usage percentage to 100% in the frontend. If needed, we can consider
    // exposing the per-engine breakdown.
    int64_t engine_runtime_ms = 0;
    if (UNSAFE_TODO(sscanf(line.data(), "\tRuntime: %" PRId64 "ms",
                           &engine_runtime_ms)) != 1) {
      continue;
    }

    busy_time += base::Milliseconds(engine_runtime_ms);
    found_runtime = true;
  }

  if (!found_runtime) {
    return std::nullopt;
  }

  return GpuInfo{.busy_time = busy_time};
}

// Checks if GPU information is available and cache the result for future calls.
bool IsGpuInfoAvailable() {
  static bool avail = [&] {
    std::optional<GpuInfo> avail = GetGpuInfoFromI915EngineInfo();
    // Assuming zero busy time means the info is unavailable as an educated
    // heuristic. It's unlikely that gpu is never used before.
    if (!avail.has_value() || avail->busy_time.is_zero()) {
      VLOG(1) << "GPU info is not available";
      return false;
    }
    return true;
  }();
  return avail;
}

std::optional<GpuInfo> GetGpuInfo() {
  if (!IsGpuInfoAvailable()) {
    return std::nullopt;
  }

  // TODO: b/380808338 - Support Mali and AMD GPU drivers.
  return GetGpuInfoFromI915EngineInfo();
}

void SetGpuValue(const std::optional<GpuInfo>& info,
                 base::Value::Dict* result) {
  if (!info.has_value()) {
    result->Set("gpu", base::Value(base::Value::Type::NONE));
    return;
  }

  int busy = ToCounter(info->busy_time.InMilliseconds());
  result->Set("gpu", base::Value::Dict().Set("busy", busy));
}

// TODO: b/380808338 - Resolve this dynamically from driver or udev instead of
// hard-coding it statically.
static constexpr char kIntelNpuBusyTimePath[] =
    "/sys/devices/pci0000:00/0000:00:0b.0/npu_busy_time_us";

bool IsNpuInfoAvailable() {
  static bool avail = [] {
    if (!base::PathIsReadable(base::FilePath(kIntelNpuBusyTimePath))) {
      VLOG(1) << "NPU info is not available";
      return false;
    }
    return true;
  }();
  return avail;
}

// TODO: b/380808338 - Support MediaTek NPU as well.
std::optional<NpuInfo> GetNpuInfo() {
  if (!IsNpuInfoAvailable()) {
    // Perform a quick IsAvailable() check for optional metrics that may not be
    // supported on all devices. This avoids unnecessary attempts to read values
    // and reduces the excessive DLOG warnings.
    return std::nullopt;
  }

  std::string content;
  if (!base::ReadFileToStringNonBlocking(base::FilePath(kIntelNpuBusyTimePath),
                                         &content)) {
    DLOG(WARNING) << "Failed to read Intel NPU busy time";
    return std::nullopt;
  }

  int64_t busy_time_us = 0;
  if (base::StringToInt64(content, &busy_time_us)) {
    DLOG(WARNING) << "Failed to parse busy time as int64";
    return std::nullopt;
  }

  return NpuInfo{.busy_time = base::Microseconds(busy_time_us)};
}

void SetNpuValue(const std::optional<NpuInfo>& info,
                 base::Value::Dict* result) {
  if (!info.has_value()) {
    result->Set("npu", base::Value(base::Value::Type::NONE));
    return;
  }

  int busy = ToCounter(info->busy_time.InMilliseconds());
  result->Set("npu", base::Value::Dict().Set("busy", busy));
}

base::Value::Dict GetSysInfo() {
  std::vector<CpuInfo> cpu_infos(base::SysInfo::NumberOfProcessors());
  if (!GetCpuInfo(&cpu_infos)) {
    DLOG(WARNING) << "Failed to get system CPU info.";
    cpu_infos.clear();
  }
  base::SystemMemoryInfo mem_info;
  if (!GetSystemMemoryInfo(&mem_info)) {
    DLOG(WARNING) << "Failed to get system memory info.";
  }
  base::VmStatInfo vmstat_info;
  if (!GetVmStatInfo(&vmstat_info)) {
    DLOG(WARNING) << "Failed to get system vmstat info.";
  }
  base::SwapInfo swap_info;
  if (!GetSwapInfo(&swap_info)) {
    DLOG(WARNING) << "Failed to get system zram info.";
  }
  std::optional<GpuInfo> gpu_info = GetGpuInfo();
  std::optional<NpuInfo> npu_info = GetNpuInfo();

  base::Value::Dict result;
  SetConstValue(&result);
  SetCpusValue(cpu_infos, &result);
  SetMemValue(mem_info, vmstat_info, &result);
  SetZramValue(swap_info, &result);
  SetGpuValue(gpu_info, &result);
  SetNpuValue(npu_info, &result);

  return result;
}

}  // namespace

SysInternalsMessageHandler::SysInternalsMessageHandler() = default;

SysInternalsMessageHandler::~SysInternalsMessageHandler() = default;

void SysInternalsMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getSysInfo",
      base::BindRepeating(&SysInternalsMessageHandler::HandleGetSysInfo,
                          base::Unretained(this)));
}

void SysInternalsMessageHandler::HandleGetSysInfo(
    const base::Value::List& list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  AllowJavascript();
  if (list.size() != 1 || !list[0].is_string()) {
    NOTREACHED();
  }

  base::Value callback_id = list[0].Clone();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&GetSysInfo),
      base::BindOnce(&SysInternalsMessageHandler::ReplySysInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)));
}

void SysInternalsMessageHandler::ReplySysInfo(base::Value callback_id,
                                              base::Value::Dict result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ResolveJavascriptCallback(callback_id, result);
}

}  // namespace ash
