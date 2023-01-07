// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/prefs_impl.h"

#include <aio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"

namespace {
constexpr char kSharedMemName[] = "/" PRODUCT_FULLNAME_STRING ".lock";
}  // namespace

namespace updater {

class ScopedPrefsLockImpl {
 public:
  ScopedPrefsLockImpl(const ScopedPrefsLockImpl&) = delete;
  ScopedPrefsLockImpl& operator=(const ScopedPrefsLockImpl&) = delete;
  ~ScopedPrefsLockImpl();

  static std::unique_ptr<ScopedPrefsLockImpl> TryCreate(
      UpdaterScope scope,
      base::TimeDelta timeout);

 private:
  ScopedPrefsLockImpl(base::raw_ptr<pthread_mutex_t> mutex, int shm_fd)
      : mutex_(mutex), shm_fd_(shm_fd) {}

  base::raw_ptr<pthread_mutex_t> mutex_;
  int shm_fd_;
};

std::unique_ptr<ScopedPrefsLockImpl> ScopedPrefsLockImpl::TryCreate(
    UpdaterScope scope,
    base::TimeDelta timeout) {
  bool should_init_mutex = false;

  int shm_fd = shm_open(kSharedMemName, O_RDWR, S_IRUSR | S_IWUSR);
  if (shm_fd < 0 && errno == ENOENT) {
    shm_fd = shm_open(kSharedMemName, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    should_init_mutex = true;
  }

  if (shm_fd < 0) {
    return nullptr;
  }

  if (ftruncate(shm_fd, sizeof(pthread_mutex_t)))
    return nullptr;

  void* addr = mmap(nullptr, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE,
                    MAP_SHARED, shm_fd, 0);
  if (addr == MAP_FAILED)
    return nullptr;
  pthread_mutex_t* mutex = static_cast<pthread_mutex_t*>(addr);

  // Note that the mutex is configured with the "robust" attribute. This ensures
  // that even if a process crashes while holding the mutex, the mutex is
  // recoverable.
  if (should_init_mutex) {
    pthread_mutexattr_t attr = {};
    if (pthread_mutexattr_init(&attr) ||
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) ||
        pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST) ||
        pthread_mutex_init(mutex, &attr)) {
      munmap(addr, sizeof(pthread_mutex_t));
      return nullptr;
    }
  }

  const base::Time start = base::Time::NowFromSystemTime();
  do {
    switch (pthread_mutex_trylock(mutex)) {
      case EOWNERDEAD: {
        // A process holding the mutex died, try to recover it.
        if (pthread_mutex_consistent(mutex))
          return nullptr;
        // The mutex is restored. It is in the locked state.
        [[fallthrough]];
      }
      case 0: {
        // The lock was acquired.
        return base::WrapUnique<ScopedPrefsLockImpl>(
            new ScopedPrefsLockImpl(mutex, shm_fd));
      }
      case EBUSY: {
        // The mutex is held by another process.
        continue;
      }
      default: {
        // An error occurred.
        return nullptr;
      }
    }
  } while (base::Time::NowFromSystemTime() - start < timeout);

  // The lock was not acquired before the timeout.
  return nullptr;
}

ScopedPrefsLockImpl::~ScopedPrefsLockImpl() {
  // TODO(crbug.com/1370140): Fix shared memory leak.
  if (mutex_) {
    pthread_mutex_unlock(mutex_.get());
    munmap(mutex_.get(), sizeof(pthread_mutex_t));
    close(shm_fd_);
  }
}

ScopedPrefsLock::ScopedPrefsLock(std::unique_ptr<ScopedPrefsLockImpl> impl)
    : impl_(std::move(impl)) {}
ScopedPrefsLock::~ScopedPrefsLock() = default;

std::unique_ptr<ScopedPrefsLock> AcquireGlobalPrefsLock(
    UpdaterScope scope,
    base::TimeDelta timeout) {
  std::unique_ptr<ScopedPrefsLockImpl> impl =
      ScopedPrefsLockImpl::TryCreate(scope, timeout);
  return impl ? std::make_unique<ScopedPrefsLock>(std::move(impl)) : nullptr;
}

}  // namespace updater
