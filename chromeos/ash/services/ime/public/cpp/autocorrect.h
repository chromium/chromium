// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_AUTOCORRECT_H_
#define CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_AUTOCORRECT_H_

namespace ash {
namespace ime {

// Must match with IMEAutocorrectSuggestionProvider in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutocorrectSuggestionProvider {
  kUnknown = 0,
  kUsEnglishPrebundled = 1,
  kUsEnglishDownloaded = 2,
  kUsEnglish840 = 3,
  kUsEnglish840V2 = 4,
  kMaxValue = kUsEnglish840V2,
};

}  // namespace ime
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_AUTOCORRECT_H_
