// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/lock.h"

#include <mach/mach.h>
#include <servers/bootstrap.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/strings/strcat.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Mach service name prefix/suffix for the global  lock.
constexpr char kLockMachServiceNamePrefix[] = MAC_BUNDLE_IDENTIFIER_STRING;
constexpr char kLockMachServiceNameSuffix[] = ".lock";

// Interval to poll for lock availability if it is not immediately available.
// Final interval will be truncated to fit the available timeout.
constexpr base::TimeDelta kLockPollingInterval = base::Seconds(3);

//
// Attempts to acquire the receive right to a named Mach service.
// Single attempt, no retries. Logs errors other than "permission denied",
// since "permission denied" typically means the service receive rights have
// already been assigned.
//
// Returns the receive right if the right was successfully acquired. If the
// right cannot be acquired for any reason, returns an invalid right instead.
base::mac::ScopedMachReceiveRight TryAcquireReceive(
    const std::string& service_name) {
  base::mac::ScopedMachReceiveRight target_right;
  kern_return_t check_in_result = bootstrap_check_in(
      bootstrap_port, service_name.c_str(),
      base::mac::ScopedMachReceiveRight::Receiver(target_right).get());
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
    return base::mac::ScopedMachReceiveRight();
  }
  return target_right;
}

// Sleeps `wait_time` until the lock should be retried, but no more than
// `kLockPollingInterval`.
void WaitToRetryLock(base::TimeDelta wait_time) {
  base::PlatformThread::Sleep(std::min(wait_time, kLockPollingInterval));
}

}  // anonymous namespace

namespace updater {

class ScopedLockImpl {
 public:
  // Constructs a ScopedLockImpl from a receive right.
  explicit ScopedLockImpl(base::mac::ScopedMachReceiveRight receive_right);

  // Releases the receive right (and therefore releases the lock).
  ~ScopedLockImpl() = default;

  ScopedLockImpl(const ScopedLockImpl&) = delete;
  ScopedLockImpl& operator=(const ScopedLockImpl&) = delete;

 private:
  // The Mach port representing the held lock itself. We only care about
  // service ownership; no messages are transferred with this port.
  base::mac::ScopedMachReceiveRight receive_right_;
};

ScopedLockImpl::ScopedLockImpl(base::mac::ScopedMachReceiveRight receive_right)
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
std::unique_ptr<ScopedLock> ScopedLock::Create(const std::string& name,
                                               UpdaterScope scope,
                                               base::TimeDelta timeout) {
  const std::string service_name(
      base::StrCat({kLockMachServiceNamePrefix, name,
                    UpdaterScopeToString(scope), kLockMachServiceNameSuffix}));

  // First, try to acquire the lock. If the timeout is zero or negative,
  // this is the only attempt we will make.
  base::mac::ScopedMachReceiveRight receive_right(
      TryAcquireReceive(service_name.c_str()));

  // Set up time limits.
  base::TimeTicks deadline = base::TimeTicks::Now() + timeout;
  constexpr base::TimeDelta kDeltaZero;

  // Loop until we acquire the lock or time out. Note that we have already
  // tried once to acquire the lock, so this loop will execute zero times
  // if we have already succeeded (or if the timeout was zero). Therefore,
  // this loop waits for retry *before* attempting to acquire the lock;
  // the special case of the first try was handled outside the loop.
  for (base::TimeDelta remain = deadline - base::TimeTicks::Now();
       !receive_right.is_valid() && remain > kDeltaZero;
       remain = deadline - base::TimeTicks::Now()) {
    WaitToRetryLock(remain);
    receive_right = TryAcquireReceive(service_name);
  }

  if (!receive_right.is_valid()) {
    return nullptr;
  }
  return std::make_unique<ScopedLock>(
      std::make_unique<ScopedLockImpl>(std::move(receive_right)));
}

}  // namespace updater
