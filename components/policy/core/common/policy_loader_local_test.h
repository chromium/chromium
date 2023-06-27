// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_LOCAL_TEST_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_LOCAL_TEST_H_

#include "components/policy/policy_export.h"

namespace policy {

class PolicyBundle;

// Loads policies from policy testing page
class POLICY_EXPORT PolicyLoaderLocalTest {
 public:
  explicit PolicyLoaderLocalTest();

  ~PolicyLoaderLocalTest();

  PolicyBundle Load();
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_LOCAL_TEST_H_
