// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_system_lock/lock.h"

#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

namespace {

// Interval to poll for lock availability if it is not immediately available.
// Final interval is truncated to fit the available timeout.
constexpr base::TimeDelta kLockPollingInterval = base::Seconds(3);

//
// Attempts to acquire the receive right to a named Mach service.
// Single attempt, no retries. Logs errors other than "permission denied",
// since "permission denied" typically means the service receive rights have
// already been assigned.
//
// Returns the receive right if the right was successfully acquired. If the
// right cannot be acquired for any reason, returns an invalid right instead.
base::apple::ScopedMachReceiveRight TryAcquireReceive(
    const base::apple::ScopedMachSendRight& bootstrap_right,
    const std::string& service_name) {
  VLOG(2) << __func__;
  base::apple::ScopedMachReceiveRight target_right;
  kern_return_t check_in_result = bootstrap_check_in(
      bootstrap_right.get(), service_name.c_str(),
      base::apple::ScopedMachReceiveRight::Receiver(target_right).get());
  if (check_in_result != KERN_SUCCESS) {
    // Log error reports for all errors other than BOOTSTRAP_NOT_PRIVILEGED.
    // BOOTSTRAP_NOT_PRIVILEGED is not logged because it just means that another
    // process has acquired the receive rights for this service.
    if (check_in_result != BOOTSTRAP_NOT_PRIVILEGED) {
      BOOTSTRAP_LOG(ERROR, check_in_result)
          << " bootstrap_check_in to acquire lock: " << service_name;
    } else {
      BOOTSTRAP_VLOG(2, check_in_result)
          << " lock already held: " << service_name;
    }
    return base::apple::ScopedMachReceiveRight();
  }
  return target_right;
}

// Sleeps `wait_time` until the lock should be retried, but no more than
// `kLockPollingInterval`.
void WaitToRetryLock(base::TimeDelta wait_time) {
  base::PlatformThread::Sleep(std::min(wait_time, kLockPollingInterval));
}

}  // anonymous namespace

namespace named_system_lock {

class ScopedLockImpl {
 public:
  // Constructs a ScopedLockImpl from a receive right.
  explicit ScopedLockImpl(base::apple::ScopedMachReceiveRight receive_right);

  // Releases the receive right.
  ~ScopedLockImpl() = default;

  ScopedLockImpl(const ScopedLockImpl&) = delete;
  ScopedLockImpl& operator=(const ScopedLockImpl&) = delete;

 private:
  // The Mach port representing the held lock itself. We only care about
  // service ownership; no messages are transferred with this port.
  base::apple::ScopedMachReceiveRight receive_right_;
};

ScopedLockImpl::ScopedLockImpl(
    base::apple::ScopedMachReceiveRight receive_right)
    : receive_right_(std::move(receive_right)) {
  mach_port_type_t port_type = 0;
  kern_return_t port_check_result =
      mach_port_type(mach_task_self(), receive_right_.get(), &port_type);
  MACH_CHECK(port_check_result == KERN_SUCCESS, port_check_result)
      << "ScopedLockImpl could not verify lock port";
  CHECK(port_type & MACH_PORT_TYPE_RECEIVE)
      << "ScopedLockImpl given port without receive right";
}

ScopedLock::ScopedLock(std::unique_ptr<ScopedLockImpl> impl)
    : impl_(std::move(impl)) {}

ScopedLock::~ScopedLock() = default;

// static
std::unique_ptr<ScopedLock> ScopedLock::Create(const std::string& service_name,
                                               base::TimeDelta timeout) {
  // Find the right namespace for the lock. Non-privileged processes cannot
  // climb "up" out of their namespace, but user processes run with the right
  // namespace (the interactive user session) anyway. System processes need the
  // system namespace, which is the root of macOS' "tree" of Mach bootstrap
  // namespaces. Daemons (including LaunchDaemons) and system services naturally
  // run under this namespace, but `sudo` -- a POSIX utility that is not aware
  // of the Mach portions of the macOS kernel -- runs targets without changing
  // their bootstrap port (and therefore their namespace). The system namespace
  // is the only one guaranteed to be shared between users.
  //
  // mac/launcher_main.c uses an equivalent algorithm to find this namespace
  // when launching a privileged process. It's repeated here so processes
  // launched via sudo (such as the integration test helper) can reach the
  // system-scope locks.
  base::apple::ScopedMachSendRight bootstrap_right =
      base::apple::RetainMachSendRight(bootstrap_port);
  if (!geteuid()) {
    // Move the initial bootstrap right into `next_right` so the first loop is
    // not a special case. base::ScopedGeneric calls `abort()` if you `reset` it
    // to what it already holds, so this has to be a move, not a retain.
    base::apple::ScopedMachSendRight next_right(bootstrap_right.release());
    while (bootstrap_right.get() != next_right.get()) {
      bootstrap_right.reset(next_right.release());
      kern_return_t bootstrap_err = bootstrap_parent(
          bootstrap_right.get(),
          base::apple::ScopedMachSendRight::Receiver(next_right).get());
      if (bootstrap_err != KERN_SUCCESS) {
        BOOTSTRAP_LOG(ERROR, bootstrap_err)
            << "can't bootstrap_parent in ScopedLock::Create for euid 0";
        break;  // Use last known bootstrap_right.
      }
      CHECK(next_right.is_valid())
          << "bootstrap_parent yielded invalid port without error";
    }
  }

  // Make one try to acquire the lock, even if the timeout is zero or negative.
  base::apple::ScopedMachReceiveRight receive_right(
      TryAcquireReceive(bootstrap_right, service_name.c_str()));

  base::TimeTicks deadline = base::TimeTicks::Now() + timeout;
  for (base::TimeDelta remain = deadline - base::TimeTicks::Now();
       !receive_right.is_valid() && remain.is_positive();
       remain = deadline - base::TimeTicks::Now()) {
    WaitToRetryLock(remain);
    receive_right = TryAcquireReceive(bootstrap_right, service_name);
  }

  if (!receive_right.is_valid()) {
    return nullptr;
  }
  return std::make_unique<ScopedLock>(
      std::make_unique<ScopedLockImpl>(std::move(receive_right)));
}

}  // namespace named_system_lock
