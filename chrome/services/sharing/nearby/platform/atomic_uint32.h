// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_ATOMIC_UINT32_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_ATOMIC_UINT32_H_

#include <atomic>

#include "third_party/nearby/src/cpp/platform/api/atomic_reference.h"

namespace location {
namespace nearby {
namespace chrome {

// Concrete AtomicUint32 implementation.
class AtomicUint32 : public api::AtomicUint32 {
 public:
  explicit AtomicUint32(std::int32_t initial_value);
  ~AtomicUint32() override;

  AtomicUint32(const AtomicUint32&) = delete;
  AtomicUint32& operator=(const AtomicUint32&) = delete;

  // api::AtomicUint32:
  std::uint32_t Get() const override;
  void Set(std::uint32_t value) override;

 private:
  std::atomic<std::uint32_t> value_;
};

}  // namespace chrome
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_ATOMIC_UINT32_H_
