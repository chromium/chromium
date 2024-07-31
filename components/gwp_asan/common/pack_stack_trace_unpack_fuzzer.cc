// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <algorithm>
#include <memory>

#include "base/containers/heap_array.h"
#include "components/gwp_asan/common/pack_stack_trace.h"

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
  base::HeapArray<uintptr_t> unpacked =
      base::HeapArray<uintptr_t>::Uninit(unpacked_array_size);
  gwp_asan::internal::Unpack(Data, Size, unpacked.data(), unpacked_max_size);
  return 0;
}
