// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_local_test.h"

#include "base/json/json_reader.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

PolicyLoaderLocalTest::PolicyLoaderLocalTest() = default;

PolicyLoaderLocalTest::~PolicyLoaderLocalTest() = default;

PolicyBundle PolicyLoaderLocalTest::Load() {
  PolicyBundle bundle;

  // TODO (b:286422730): Load policies from json file.

  return bundle;
}

}  // namespace policy
