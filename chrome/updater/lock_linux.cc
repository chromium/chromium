// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/lock.h"

#include <aio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"

namespace {
constexpr char kSharedMemNamePrefix[] = "/" PRODUCT_FULLNAME_STRING;
constexpr char kSharedMemNameSuffix[] = ".lock";
}  // namespace

namespace updater {

// A global preferences lock for Linux implemented with pthread mutexes in
// shared memory. Note that the shared memory region is leaked once per lock
// name for reasons described in `//docs/updater/design_doc.md`. The size of the
// leaked region is ~40 bytes.
class ScopedLockImpl {
 public:
  ScopedLockImpl(const ScopedLockImpl&) = delete;
  ScopedLockImpl& operator=(const ScopedLockImpl&) = delete;
  ~ScopedLockImpl();

  static std::unique_ptr<ScopedLockImpl> TryCreate(const std::string& id,
                                                   UpdaterScope scope,
                                                   base::TimeDelta timeout);

 private:
  ScopedLockImpl(pthread_mutex_t* mutex, int shm_fd)
      : mutex_(mutex), shm_fd_(shm_fd) {}

  // This field is not a raw_ptr<> because it always points to a mmap'd
  // region of memory outside of the PA heap. Thus, there would be overhead
  // involved with using a raw_ptr<> but no safety gains.
  RAW_PTR_EXCLUSION pthread_mutex_t* mutex_;
  int shm_fd_;
};

std::unique_ptr<ScopedLockImpl> ScopedLockImpl::TryCreate(
    const std::string& id,
    UpdaterScope scope,
    base::TimeDelta timeout) {
  bool should_init_mutex = false;
  const std::string shared_mem_name(
      base::StrCat({kSharedMemNamePrefix, id, UpdaterScopeToString(scope),
                    kSharedMemNameSuffix}));

  int shm_fd = shm_open(shared_mem_name.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
  if (shm_fd < 0 && errno == ENOENT) {
    shm_fd =
        shm_open(shared_mem_name.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    should_init_mutex = true;
  }

  if (shm_fd < 0) {
    return nullptr;
  }

  base::stat_wrapper_t shm_stat;
  if (base::File::Fstat(shm_fd, &shm_stat) < 0) {
    PLOG(ERROR) << "Cannot stat shared memory " << shared_mem_name;
    return nullptr;
  }
  if (shm_stat.st_uid != getuid() ||
      (shm_stat.st_mode & 0777) != (S_IRUSR | S_IWUSR)) {
    LOG(ERROR) << "Refusing to use shared memory region " << shared_mem_name
               << " with incorrect permissions";
    return nullptr;
  }

  if (ftruncate(shm_fd, sizeof(pthread_mutex_t))) {
    return nullptr;
  }

  void* addr = mmap(nullptr, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE,
                    MAP_SHARED, shm_fd, 0);
  if (addr == MAP_FAILED) {
    return nullptr;
  }
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
        if (pthread_mutex_consistent(mutex)) {
          return nullptr;
        }
        // The mutex is restored. It is in the locked state.
        [[fallthrough]];
      }
      case 0: {
        // The lock was acquired.
        return base::WrapUnique<ScopedLockImpl>(
            new ScopedLockImpl(mutex, shm_fd));
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

ScopedLockImpl::~ScopedLockImpl() {
  if (mutex_) {
    pthread_mutex_unlock(mutex_);
    munmap(mutex_, sizeof(pthread_mutex_t));
    close(shm_fd_);
  }
}

ScopedLock::ScopedLock(std::unique_ptr<ScopedLockImpl> impl)
    : impl_(std::move(impl)) {}
ScopedLock::~ScopedLock() = default;

// static
std::unique_ptr<ScopedLock> ScopedLock::Create(const std::string& name,
                                               UpdaterScope scope,
                                               base::TimeDelta timeout) {
  std::unique_ptr<ScopedLockImpl> impl =
      ScopedLockImpl::TryCreate(name, scope, timeout);
  return impl ? std::make_unique<ScopedLock>(std::move(impl)) : nullptr;
}

}  // namespace updater
