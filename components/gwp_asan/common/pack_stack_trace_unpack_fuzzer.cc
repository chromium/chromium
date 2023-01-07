// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/common/pack_stack_trace.h"

#include <algorithm>
#include <memory>

// Tests that Unpack() correctly handles arbitrary inputs.

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  // The first sizeof(size_t) bytes of the input are treated as the
  // unpacked_max_size argument to Unpack.
  if (Size < sizeof(size_t))
    return 0;

  size_t unpacked_max_size = *reinterpret_cast<const size_t*>(Data);
  Data += sizeof(size_t);
  Size -= sizeof(size_t);

  // This should always be large enough to hold the output.
  size_t unpacked_array_size = std::min(Size, unpacked_max_size);
  std::unique_ptr<uintptr_t[]> unpacked(new uintptr_t[unpacked_array_size]);
  gwp_asan::internal::Unpack(Data, Size, unpacked.get(), unpacked_max_size);
  return 0;
}
