// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/prefs_impl.h"

#include <mach/mach.h>
#include <servers/bootstrap.h>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"

namespace {

// Mach service name for the global prefs lock.
constexpr char kPrefsLockMachServiceName[] =
    MAC_BUNDLE_IDENTIFIER_STRING ".lock";

// Interval to poll for lock availability if it is not immediately available.
// Final interval will be truncated to fit the available timeout.
constexpr base::TimeDelta kPrefsLockPollingInterval = base::Seconds(3);

//
// Attempts to acquire the receive right to a named Mach service.
// Single attempt, no retries. Logs errors other than "permission denied",
// since "permission denied" typically means the service receive rights have
// already been assigned.
//
// Returns the receive right if the right was successfully acquired. If the
// right cannot be acquired for any reason, returns an invalid right instead.
base::mac::ScopedMachReceiveRight TryAcquireReceive(const char* service_name) {
  base::mac::ScopedMachReceiveRight target_right;
  kern_return_t check_in_result = bootstrap_check_in(
      bootstrap_port, service_name,
      base::mac::ScopedMachReceiveRight::Receiver(target_right).get());
  if (check_in_result != KERN_SUCCESS) {
    // Log error reports for all errors other than BOOTSTRAP_NOT_PRIVILEGED.
    // BOOTSTRAP_NOT_PRIVILEGED is not logged because it just means that another
    // process has acquired the receive rights for this service.
    if (check_in_result != BOOTSTRAP_NOT_PRIVILEGED) {
      BOOTSTRAP_LOG(ERROR, check_in_result)
          << "bootstrap_check_in to acquire prefs lock: "
          << kPrefsLockMachServiceName;
    } else {
      BOOTSTRAP_VLOG(2, check_in_result)
          << "Prefs lock already held: " << kPrefsLockMachServiceName;
    }
    return base::mac::ScopedMachReceiveRight();
  }
  return target_right;
}

// Sleep until the lock should be retried, up to an approximate maximum of
// max_wait (within the tolerances of timing, scheduling, etc.).
void WaitToRetryLock(base::TimeDelta max_wait) {
  // This is a polling implementation of Mach service locking.
  // TODO(1135787): replace with a non-polling Mach notification approach.
  const base::TimeDelta wait_time = max_wait < kPrefsLockPollingInterval
                                        ? max_wait
                                        : kPrefsLockPollingInterval;
  base::PlatformThread::Sleep(wait_time);
}

}  // anonymous namespace

namespace updater {

class ScopedPrefsLockImpl {
 public:
  // Constructs a ScopedPrefsLockImpl from a receive right.
  explicit ScopedPrefsLockImpl(base::mac::ScopedMachReceiveRight receive_right);

  // Releases the receive right (and therefore releases the lock).
  ~ScopedPrefsLockImpl() = default;

  ScopedPrefsLockImpl(const ScopedPrefsLockImpl&) = delete;
  ScopedPrefsLockImpl& operator=(const ScopedPrefsLockImpl&) = delete;

 private:
  // The Mach port representing the held lock itself. We only care about
  // service ownership; no messages are transfered with this port.
  base::mac::ScopedMachReceiveRight receive_right_;
};

ScopedPrefsLockImpl::ScopedPrefsLockImpl(
    base::mac::ScopedMachReceiveRight receive_right)
    : receive_right_(std::move(receive_right)) {
  mach_port_type_t port_type = 0;
  kern_return_t port_check_result =
      mach_port_type(mach_task_self(), receive_right_.get(), &port_type);
  MACH_CHECK(port_check_result == KERN_SUCCESS, port_check_result)
      << "ScopedPrefsLockImpl could not verify lock port";
  CHECK(port_type & MACH_PORT_TYPE_RECEIVE)
      << "ScopedPrefsLockImpl given port without receive right";
}

ScopedPrefsLock::ScopedPrefsLock(std::unique_ptr<ScopedPrefsLockImpl> impl)
    : impl_(std::move(impl)) {}

ScopedPrefsLock::~ScopedPrefsLock() = default;

std::unique_ptr<ScopedPrefsLock> AcquireGlobalPrefsLock(
    UpdaterScope scope,
    base::TimeDelta timeout) {
  // First, try to acquire the lock. If the timeout is zero or negative,
  // this is the only attempt we will make.
  base::mac::ScopedMachReceiveRight receive_right(
      TryAcquireReceive(kPrefsLockMachServiceName));

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
    receive_right = TryAcquireReceive(kPrefsLockMachServiceName);
  }

  if (!receive_right.is_valid()) {
    return nullptr;
  }
  return std::make_unique<ScopedPrefsLock>(
      std::make_unique<ScopedPrefsLockImpl>(std::move(receive_right)));
}

}  // namespace updater
