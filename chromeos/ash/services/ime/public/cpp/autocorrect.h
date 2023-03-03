// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_AUTOCORRECT_H_
#define CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_AUTOCORRECT_H_

namespace ash {
namespace ime {

// Specifies the model that provides autocorrect suggestions.
// TODO(b/270090531): Record UMA metric for these values whenever an input is
// focused.
enum class AutocorrectSuggestionProvider {
  kUnknown = 0,
  kUsEnglishPrebundled = 1,
  kUsEnglishDownloaded = 2,
  kUsEnglish840 = 3,
};

}  // namespace ime
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_AUTOCORRECT_H_
