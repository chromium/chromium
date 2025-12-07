// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/suggestions_context.h"

#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

SuggestionsContext::SuggestionsContext() = default;
SuggestionsContext::SuggestionsContext(const SuggestionsContext&) = default;
SuggestionsContext& SuggestionsContext::operator=(const SuggestionsContext&) =
    default;
SuggestionsContext::~SuggestionsContext() = default;

}  // namespace autofill
