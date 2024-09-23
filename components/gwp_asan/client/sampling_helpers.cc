// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/sampling_helpers.h"

#include <optional>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/strcat.h"

namespace gwp_asan::internal {

namespace {

// The exclusive maximum-valued sample that the histogram supports.
// Arbitrarily chosen.
inline constexpr base::Histogram::Sample kMaxSample = 1'000'000'000;

constexpr auto kProcessMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>(
        {{"", "Browser"},
         {"extension", "Extension"},
         {"gpu-process", "Gpu"},
         {"ppapi", "Ppapi"},
         {"renderer", "Renderer"},
         {"utility", "Utility"}});

void ReportOomHistogram(base::HistogramBase* histogram,
                        size_t sampling_frequency,
                        size_t allocations) {
  base::ClampedNumeric<base::Histogram::Sample> total_allocations = allocations;
  total_allocations *= sampling_frequency;

  // The safety provided by `ClampedNumeric` is well and good, but we
  // arbitrarily chose an exclusive maximum for our histogram and have
  // to limbo under it.
  histogram->Add(total_allocations.Min(kMaxSample - 1));
}

}  // namespace

GuardedPageAllocator::OutOfMemoryCallback CreateOomCallback(
    std::string_view allocator_name,
    std::string_view process_type,
    size_t sampling_frequency) {
  const auto process_str = ProcessString(process_type);
  if (!process_str.has_value()) {
    return base::DoNothing();
  }

  // N.B. we call `FactoryGet()` here to avoid doing it inside the
  // callback body, which could result in re-entrancy issues.
  // See https://crbug.com/331729344 for details.
  const std::string histogram_name =
      base::StrCat({"Security.GwpAsan.AllocatorOom.", allocator_name, ".",
                    process_str.value()});
  auto* histogram = base::Histogram::FactoryGet(
      histogram_name,
      /*minimum=*/1,
      /*maximum=*/kMaxSample,
      /*bucket_count=*/100,
      /*flags=*/base::HistogramBase::Flags::kUmaTargetedHistogramFlag);
  CHECK(histogram);
  return base::BindOnce(&ReportOomHistogram, histogram, sampling_frequency);
}

// Turns the value passed to --type= into
// *  a representation of the process type
// *  or `nullopt` if we don't emit for said process.
//
// Used in the patterned histogram `Security.GwpAsan.AllocatorOom...`.
std::optional<std::string_view> ProcessString(std::string_view process_type) {
  const auto iter = kProcessMap.find(process_type);
  if (iter == kProcessMap.cend()) {
    return std::nullopt;
  }
  return std::make_optional(iter->second);
}

void ReportGwpAsanActivated(std::string_view allocator_name,
                            std::string_view process_type,
                            bool activated) {
  const auto process_str = ProcessString(process_type);
  if (!process_str.has_value()) {
    return;
  }
  const std::string histogram_name =
      base::StrCat({"Security.GwpAsan.Activated.", allocator_name, ".",
                    process_str.value()});
  base::UmaHistogramBoolean(histogram_name, activated);
}

}  // namespace gwp_asan::internal
