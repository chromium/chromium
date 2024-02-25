// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "components/reporting/util/status_macros.h"

#include "base/check.h"

namespace reporting {
namespace {

void CheckOtherTypes(char foo) {
  // Should fail because foo is not neither Status or StatusOr.
  CHECK_OK(foo);  // expected-error@components/reporting/util/status_macros.h:* {{ {CHECK,DCHECK,ASSERT,EXPECT}_OK do not accept a type other than Status or StatusOr.}}
}
}  // namespace
}  // namespace reporting
