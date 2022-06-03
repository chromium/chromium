// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/constants/constants_util.h"

#include "base/no_destructor.h"

namespace chromeos {
namespace settings {
namespace constants {
namespace {

template <typename T>
std::vector<T> All() {
  int32_t min_value = static_cast<int32_t>(T::kMinValue);
  int32_t max_value = static_cast<int32_t>(T::kMaxValue);

  std::vector<T> all;
  for (int32_t i = min_value; i <= max_value; ++i) {
    T current = static_cast<T>(i);

    // Not every value between the min and max values is valid:
    // (1) We use a numbering scheme which purposely skips some values for the
    //     Subpage and Setting enums.
    // (2) Some values are deprecated and removed.
    if (mojom::IsKnownEnumValue(current))
      all.push_back(current);
  }

  return all;
}

}  // namespace

const std::vector<mojom::Section>& AllSections() {
  static const base::NoDestructor<std::vector<mojom::Section>> all_sections(
      All<mojom::Section>());
  return *all_sections;
}

const std::vector<mojom::Subpage>& AllSubpages() {
  static const base::NoDestructor<std::vector<mojom::Subpage>> all_subpages(
      All<mojom::Subpage>());
  return *all_subpages;
}

const std::vector<mojom::Setting>& AllSettings() {
  static const base::NoDestructor<std::vector<mojom::Setting>> all_settings(
      All<mojom::Setting>());
  return *all_settings;
}

}  // namespace constants
}  // namespace settings
}  // namespace chromeos
