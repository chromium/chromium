// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_INPUT_METHOD_TEST_INTERFACE_CONSTANTS_H_
#define CHROMEOS_CROSAPI_CPP_INPUT_METHOD_TEST_INTERFACE_CONSTANTS_H_

#include "base/component_export.h"

namespace crosapi {

// Any constants related to Lacros input method browser tests goes here.
// Typically this is used for strings representing "test capabilities" to check
// for version skew. See `InputMethodTestInterface::HasCapabilities`.
// This might be temporarily empty if test capabilities were removed over time
// when older versions of Ash are no longer tested against Lacros.

}  // namespace crosapi

#endif  // CHROMEOS_CROSAPI_CPP_INPUT_METHOD_TEST_INTERFACE_CONSTANTS_H_
