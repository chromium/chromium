// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_TEST_PATTERN_PROVIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_TEST_PATTERN_PROVIDER_H_

#include "components/autofill/core/browser/pattern_provider/pattern_provider.h"

namespace autofill {

// The pattern provider to be used in tests. Loads the MatchingPattern
// configuration synchronously from the Resource Bundle and sets itself as the
// global PatternProvider.
class TestPatternProvider : public PatternProvider {
 public:
  TestPatternProvider();
  ~TestPatternProvider();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_TEST_PATTERN_PROVIDER_H_
