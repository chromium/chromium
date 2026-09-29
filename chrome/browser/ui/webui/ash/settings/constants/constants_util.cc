// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/constants/constants_util.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "base/containers/span.h"

namespace ash::settings {
namespace {

namespace mojom {
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

template <typename T, auto Filter = [](T) { return true; }>
consteval auto All() {
  constexpr int32_t kMinValue = static_cast<int32_t>(T::kMinValue);
  constexpr int32_t kMaxValue = static_cast<int32_t>(T::kMaxValue);

  constexpr size_t kCount = [] {
    size_t count = 0;
    for (int32_t i = kMinValue; i <= kMaxValue; ++i) {
      T current = static_cast<T>(i);

      // Not every value between the min and max values is valid:
      // (1) We use a numbering scheme which purposely skips some values for the
      //     Subpage and Setting enums.
      // (2) Some values are deprecated and removed.
      if (chromeos::settings::mojom::IsKnownEnumValue(current) &&
          Filter(current)) {
        ++count;
      }
    }
    return count;
  }();

  std::array<T, kCount> all{};
  size_t index = 0;
  for (int32_t i = kMinValue; i <= kMaxValue; ++i) {
    T current = static_cast<T>(i);
    if (chromeos::settings::mojom::IsKnownEnumValue(current) &&
        Filter(current)) {
      all[index++] = current;
    }
  }

  return all;
}

}  // namespace

base::span<const mojom::Section> AllSections() {
  static constexpr auto kAllSections = All<mojom::Section>();
  return kAllSections;
}

base::span<const mojom::Subpage> AllSubpages() {
  static constexpr auto kAllSubpages =
      All<mojom::Subpage, [](mojom::Subpage subpage) {
        return subpage != mojom::Subpage::kInternalStorybook;
      }>();
  return kAllSubpages;
}

base::span<const mojom::Setting> AllSettings() {
  static constexpr auto kAllSettings = All<mojom::Setting>();
  return kAllSettings;
}

}  // namespace ash::settings
