// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/sampling_helpers.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"

namespace gwp_asan {
namespace internal {

namespace {

// Turns the value passed to --type= into a string used in a histogram name for
// common processes, or nullptr otherwise.
const char* ProcessString(const char* process_type) {
  if (!process_type)
    return nullptr;
  if (!strcmp(process_type, ""))
    return "Browser";
  if (!strcmp(process_type, "extension"))
    return "Extension";
  if (!strcmp(process_type, "gpu-process"))
    return "Gpu";
  if (!strcmp(process_type, "ppapi"))
    return "Ppapi";
  if (!strcmp(process_type, "renderer"))
    return "Renderer";
  if (!strcmp(process_type, "utility"))
    return "Utility";
  return nullptr;
}

void ReportOomHistogram(std::string histogram_name,
                        size_t sampling_frequency,
                        size_t allocations) {
  base::CheckedNumeric<int> total_allocations = allocations;
  total_allocations *= sampling_frequency;
  if (total_allocations.IsValid()) {
    base::UmaHistogramCustomCounts(/*name=*/histogram_name,
                                   /*sample=*/total_allocations.ValueOrDie(),
                                   /*min=*/1, /*max=*/100000000,
                                   /*buckets=*/100);
  }
}

}  // namespace

GuardedPageAllocator::OutOfMemoryCallback CreateOomCallback(
    const char* allocator_name,
    const char* process_type,
    size_t sampling_frequency) {
  const char* process_str = ProcessString(process_type);
  if (!process_str)
    return base::DoNothing();

  std::string histogram_name = base::StringPrintf("GwpAsan.AllocatorOom.%s.%s",
                                                  allocator_name, process_str);
  return base::BindOnce(&ReportOomHistogram, std::move(histogram_name),
                        sampling_frequency);
}

}  // namespace internal
}  // namespace gwp_asan
