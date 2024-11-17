// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_ATOMIC_UINT32_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_ATOMIC_UINT32_H_

#include <atomic>

#include "third_party/nearby/src/internal/platform/implementation/atomic_reference.h"

namespace nearby::chrome {

// Concrete AtomicUint32 implementation.
class AtomicUint32 : public api::AtomicUint32 {
 public:
  explicit AtomicUint32(std::uint32_t initial_value);
  ~AtomicUint32() override;

  AtomicUint32(const AtomicUint32&) = delete;
  AtomicUint32& operator=(const AtomicUint32&) = delete;

  // api::AtomicUint32:
  std::uint32_t Get() const override;
  void Set(std::uint32_t value) override;

 private:
  std::atomic<std::uint32_t> value_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_ATOMIC_UINT32_H_
