// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_DIACRITIC_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_DIACRITIC_UTILS_H_

#include <string>

namespace ash::string_matching {

// Removes diacritics from 'str' and return the result. e.g.:
// RemoveDiacritics(hölzle) => holzle
const std::u16string RemoveDiacritics(const std::u16string& str);

}  // namespace ash::string_matching
#endif  // CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_DIACRITIC_UTILS_H_
