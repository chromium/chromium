// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_TEST_UTILS_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_TEST_UTILS_H_

#include "testing/gmock/include/gmock/gmock.h"

MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_TEST_UTILS_H_
