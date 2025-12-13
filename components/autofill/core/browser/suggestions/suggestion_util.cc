// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/suggestion_util.h"

#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"

namespace autofill {

bool SuppressSuggestionsForAutocompleteUnrecognizedField(
    const AutofillField& field) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return false;
#else
  return field.ShouldSuppressSuggestionsAndFillingByDefault();
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

}  // namespace autofill
