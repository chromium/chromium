// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element_finder_result.h"

namespace autofill_assistant {

ElementFinderResult::ElementFinderResult() = default;

ElementFinderResult::~ElementFinderResult() = default;

ElementFinderResult::ElementFinderResult(const ElementFinderResult&) = default;

ElementFinderResult ElementFinderResult::EmptyResult() {
  return ElementFinderResult();
}

}  // namespace autofill_assistant
