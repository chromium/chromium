// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/core_scheduling.h"

#include <errno.h>
#include <sys/prctl.h>

#include <set>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_restrictions.h"

// Downstream core scheduling interface for 4.19, 5.4 kernels.
// TODO(b/152605392): Remove once those kernel versions are obsolete.
#ifndef PR_SET_CORE_SCHED
#define PR_SET_CORE_SCHED 0x200
#endif

// Upstream interface for core scheduling.
#ifndef PR_SCHED_CORE
#define PR_SCHED_CORE 62
#define PR_SCHED_CORE_GET 0
#define PR_SCHED_CORE_CREATE 1
#define PR_SCHED_CORE_SHARE_TO 2
#define PR_SCHED_CORE_SHARE_FROM 3
#define PR_SCHED_CORE_MAX 4
#endif
#ifndef PID_T_MAX
#define PID_T_MAX (1 << 30)
#endif

enum pid_type { PIDTYPE_PID = 0, PIDTYPE_TGID, PIDTYPE_PGID };

namespace chromeos {
namespace system {

namespace {
BASE_FEATURE(kCoreScheduling,
             "CoreSchedulingEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);
}

void EnableCoreSchedulingIfAvailable() {
  if (!base::FeatureList::IsEnabled(kCoreScheduling)) {
    return;
  }

  // prctl(2) will return EINVAL for unknown functions. We're tolerant to this
  // and will log an error message for non EINVAL errnos.
  if (prctl(PR_SET_CORE_SCHED, 1) == -1 &&
      prctl(PR_SCHED_CORE, PR_SCHED_CORE_CREATE, 0, PIDTYPE_PID, 0) == -1) {
    PLOG_IF(WARNING, errno != EINVAL) << "Unable to set core scheduling";
  }
}

bool IsCoreSchedulingAvailable() {
  static const bool kernel_support = []() {
    // Test for kernel 4.19, 5.4 downstream support.
    // Pass bad param `prctl(0x200, 2)`. If it is supported, we will get ERANGE
    // rather than EINVAL.
    if (prctl(PR_SET_CORE_SCHED, 2) == -1 && errno == ERANGE) {
      VLOG(1) << "Core scheduling legacy interface supported";
      return true;
    }

    // Test for kernel 5.10 upstream support.
    // Pass bad param pid=PID_T_MAX. If it is supported, we will get ENODEV
    // (supported by not enabled) or ESRCH (bad param).
    int res =
        prctl(PR_SCHED_CORE, PR_SCHED_CORE_CREATE, PID_T_MAX, PIDTYPE_PID, 0);
    int saved_errno = errno;
    if (res == -1 && saved_errno == ENODEV) {
      VLOG(1) << "Core scheduling supported, but not enabled "
                 "(likely because SMT is not enabled)";
      return false;
    }
    if (res == -1 && saved_errno == ESRCH) {
      VLOG(1) << "Core scheduling supported and enabled";
      return true;
    }
    VLOG(1) << "Core scheduling not supported in kernel";
    return false;
  }();
  if (!kernel_support) {
    return false;
  }

  static const bool has_vulns = []() {
    // Reading from sysfs doesn't block.
    base::ScopedAllowBlocking scoped_allow_blocking;

    base::FilePath sysfs_vulns("/sys/devices/system/cpu/vulnerabilities");
    std::string buf;
    for (const std::string& s : {"l1tf", "mds"}) {
      base::FilePath vuln = sysfs_vulns.Append(s);
      if (!base::ReadFileToString(vuln, &buf)) {
        LOG(ERROR) << "Could not read " << vuln;
        continue;
      }
      base::TrimWhitespaceASCII(buf, base::TRIM_ALL, &buf);
      if (buf != "Not affected") {
        VLOG(1) << "Core scheduling available: " << vuln << "=" << buf;
        return true;
      }
    }
    VLOG(1) << "Core scheduling not required";
    return false;
  }();
  return has_vulns;
}

int NumberOfPhysicalCores() {
  // cat /sys/devices/system/cpu/cpu[0-9]*/topology/thread_siblings_list |\
  //   sort -u | wc -l
  static const int num_physical_cores = []() {
    // Reading from sysfs doesn't block.
    base::ScopedAllowBlocking scoped_allow_blocking;

    std::set<std::string> lists;
    std::string buf;
    for (int i = 0; i < base::SysInfo::NumberOfProcessors(); ++i) {
      base::FilePath list(base::StringPrintf(
          "/sys/devices/system/cpu/cpu%d/topology/thread_siblings_list", i));
      if (!base::ReadFileToString(list, &buf)) {
        LOG(ERROR) << "Could not read " << list;
      } else {
        lists.insert(buf);
      }
    }
    return lists.size() > 0 ? lists.size() : 1;
  }();
  return num_physical_cores;
}

}  // namespace system
}  // namespace chromeos
