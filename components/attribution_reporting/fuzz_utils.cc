// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/fuzz_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/source_type.mojom-shared.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace attribution_reporting {

namespace {

fuzztest::Domain<std::string> AnyFilterString(std::optional<size_t> max_size) {
  auto string = fuzztest::Arbitrary<std::string>();
  if (max_size.has_value()) {
    string.WithMaxSize(*max_size);
  }

  return fuzztest::Filter(
      [](const std::string& s) {
        return base::IsStringUTF8AllowingNoncharacters(s);
      },
      std::move(string));
}

template <typename KeyPred>
fuzztest::Domain<FilterValues> AnyFilterValues(
    const fuzztest::Domain<std::string>& string,
    KeyPred&& key_pred,
    std::optional<size_t> max_values_per_key,
    std::optional<size_t> max_keys) {
  auto values_per_key = fuzztest::UniqueElementsVectorOf(string);
  if (max_values_per_key.has_value()) {
    values_per_key.WithMaxSize(*max_values_per_key);
  }

  auto key_values = fuzztest::ContainerOf<FilterValues>(
      fuzztest::PairOf(fuzztest::Filter(std::move(key_pred), string),
                       std::move(values_per_key)));
  if (max_keys.has_value()) {
    key_values.WithMaxSize(*max_keys);
  }

  return key_values;
}

fuzztest::Domain<base::TimeDelta> AnyLookbackWindow() {
  return fuzztest::Map(
      [](int64_t micros) { return base::Microseconds(micros); },
      fuzztest::Positive<int64_t>());
}

}  // namespace

fuzztest::Domain<mojom::SourceType> AnySourceType() {
  return fuzztest::ElementOf<mojom::SourceType>({
      mojom::SourceType::kNavigation,
      mojom::SourceType::kEvent,
  });
}

fuzztest::Domain<MaxEventLevelReports> AnyMaxEventLevelReports() {
  return fuzztest::ConstructorOf<MaxEventLevelReports>(
      fuzztest::InRange(0, static_cast<int>(MaxEventLevelReports::Max())));
}

fuzztest::Domain<FilterData> AnyFilterData() {
  return fuzztest::Map(
      [](FilterValues&& filter_values) {
        return *FilterData::Create(std::move(filter_values));
      },
      AnyFilterValues(
          AnyFilterString(kMaxBytesPerFilterString),
          [](const std::string& key) {
            return key != FilterData::kSourceTypeFilterKey;
          },
          kMaxValuesPerFilter, kMaxFiltersPerSource));
}

fuzztest::Domain<FilterConfig> AnyFilterConfig() {
  return fuzztest::Map(
      [](FilterValues&& filter_values,
         std::optional<base::TimeDelta> lookback_window) {
        return *FilterConfig::Create(std::move(filter_values), lookback_window);
      },
      AnyFilterValues(
          AnyFilterString(/*max_size=*/std::nullopt),
          [](const std::string& key) {
            return !base::StartsWith(key, FilterConfig::kReservedKeyPrefix);
          },
          /*max_values_per_key=*/std::nullopt,
          /*max_keys=*/std::nullopt),
      fuzztest::OptionalOf(AnyLookbackWindow()));
}

fuzztest::Domain<FiltersDisjunction> AnyFiltersDisjunction() {
  return fuzztest::VectorOf(AnyFilterConfig());
}

fuzztest::Domain<FilterPair> AnyFilterPair() {
  return fuzztest::ConstructorOf<FilterPair>(
      /*positive=*/AnyFiltersDisjunction(),
      /*negative=*/AnyFiltersDisjunction());
}

}  // namespace attribution_reporting
