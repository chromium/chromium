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

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/process/process_metrics.h"
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
  int vals =
      sscanf(line.c_str(),
             "cpu%" PRIu32 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64,
             &cpu_index, &user, &nice, &sys, &idle);
  if (vals != 5 || cpu_index >= infos->size()) {
    NOTREACHED_IN_MIGRATION();
    return false;
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
  if (!base::ReadFileToString(base::FilePath(kProcStat), &contents))
    return false;

  std::istringstream iss(contents);
  std::string line;

  // Skip the first line because it is just an aggregated number of
  // all cpuN lines.
  std::getline(iss, line);
  while (std::getline(iss, line)) {
    if (line.compare(0, 3, "cpu") != 0)
      continue;
    if (!ParseProcStatLine(line, infos))
      return false;
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

const double kBytesInKB = 1024;

double GetAvailablePhysicalMemory(const base::SystemMemoryInfoKB& info) {
  double available = static_cast<double>(
      info.available == 0 ? info.free + info.reclaimable : info.available);

  return available * kBytesInKB;
}

void SetMemValue(const base::SystemMemoryInfoKB& info,
                 const base::VmStatInfo& vmstat,
                 base::Value::Dict* result) {
  DCHECK(result);
  base::Value::Dict mem_result;

  // For values that may exceed the range of 32-bit signed integer, use double.
  double total = static_cast<double>(info.total) * kBytesInKB;
  mem_result.Set("total", total);
  mem_result.Set("available", GetAvailablePhysicalMemory(info));
  double swap_total = static_cast<double>(info.swap_total) * kBytesInKB;
  mem_result.Set("swapTotal", swap_total);
  double swap_free = static_cast<double>(info.swap_free) * kBytesInKB;
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

base::Value::Dict GetSysInfo() {
  std::vector<CpuInfo> cpu_infos(base::SysInfo::NumberOfProcessors());
  if (!GetCpuInfo(&cpu_infos)) {
    DLOG(WARNING) << "Failed to get system CPU info.";
    cpu_infos.clear();
  }
  base::SystemMemoryInfoKB mem_info;
  if (!GetSystemMemoryInfo(&mem_info)) {
    DLOG(WARNING) << "Failed to get system memory info.";
  }
  base::VmStatInfo vmstat_info;
  if (!GetVmStatInfo(&vmstat_info)) {
    DLOG(WARNING) << "Failed to get system vmstat info.";
  }
  base::SwapInfo swap_info;
  if (!GetSwapInfo(&swap_info)) {
    DLOG(WARNING) << ("Failed to get system zram info.");
  }

  base::Value::Dict result;
  SetConstValue(&result);
  SetCpusValue(cpu_infos, &result);
  SetMemValue(mem_info, vmstat_info, &result);
  SetZramValue(swap_info, &result);

  return result;
}

}  // namespace

SysInternalsMessageHandler::SysInternalsMessageHandler() {}

SysInternalsMessageHandler::~SysInternalsMessageHandler() {}

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
    NOTREACHED_IN_MIGRATION();
    return;
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
