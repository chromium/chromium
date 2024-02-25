// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_LANGUAGE_CODE_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_LANGUAGE_CODE_H_

#include <string>

#include "base/types/strong_alias.h"

namespace autofill {

// A language code is a lowercase alphabetic string of length up to 3, with the
// exception of "zh-*", in particular "zh-CN" and "zh-TW", and "mni-*", in
// particular "mni-Mtei". A non-exhaustive list of known languages is
// translate::kDefaultSupportedLanguages.
//
// The string "und" represents an undetermined or unknown language. For some
// reason, we sometimes also sometimes see "unknown" and perhaps other strings.
// See crbug.com/1423819 for an inconclusive discussion.
//
// C++ small string optimization keeps these objects lightweight so that copying
// should not be a worry.
using LanguageCode = base::StrongAlias<class LanguageCodeTag, std::string>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_LANGUAGE_CODE_H_
