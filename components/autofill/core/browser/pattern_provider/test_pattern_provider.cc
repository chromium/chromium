// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/pattern_provider/test_pattern_provider.h"

#include "components/autofill/core/browser/pattern_provider/pattern_configuration_parser.h"

namespace autofill {

TestPatternProvider::TestPatternProvider() {
  base::Optional<PatternProvider::Map> patterns =
      field_type_parsing::GetPatternsFromResourceBundleSynchronously();
  if (patterns)
    SetPatterns(patterns.value(), base::Version(), true);

  PatternProvider::SetPatternProviderForTesting(this);
}

TestPatternProvider::~TestPatternProvider() {
  PatternProvider::ResetPatternProvider();
}

}  // namespace autofill
