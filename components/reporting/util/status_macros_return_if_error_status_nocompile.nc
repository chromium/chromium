// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "components/reporting/util/status_macros.h"

#include "base/check.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

int ReturnIfOtherTypes(int foo) {
  // Should fail because foo is not neither Status or base::unexpected<Status>.
  RETURN_IF_ERROR_STATUS(foo);  // expected-error {{variable has incomplete type 'void'}}
                                // expected-error@components/reporting/util/status_macros.h:* {{RETURN_IF_ERROR_STATUS only accepts either Status or base::unexpected<Status>.}}

  return 0;
}
}  // namespace
}  // namespace reporting
