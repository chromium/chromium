// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_MUTEX_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_MUTEX_H_

#include "base/synchronization/lock.h"
#include "third_party/nearby/src/internal/platform/implementation/mutex.h"

namespace nearby::chrome {

// Concrete Mutex implementation. Non-recursive lock.
class Mutex : public api::Mutex {
 public:
  Mutex();
  ~Mutex() override;

  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  // api::Mutex:
  void Lock() override;
  void Unlock() override;

 private:
  friend class ConditionVariable;
  base::Lock lock_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_MUTEX_H_
