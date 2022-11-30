// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_ALIGNED_MEMORY_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_ALIGNED_MEMORY_H_

#include <type_traits>

#include "base/memory/aligned_memory.h"
#include "base/memory/page_size.h"

namespace ash {
namespace memory {

template <typename Type>
inline bool IsPageAligned(Type val) {
  static_assert(std::is_integral<Type>::value || std::is_pointer<Type>::value,
                "Integral or pointer type required");
  return base::IsAligned(val, base::GetPageSize());
}

}  // namespace memory
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_ALIGNED_MEMORY_H_
