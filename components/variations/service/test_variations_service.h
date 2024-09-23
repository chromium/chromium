// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_TEST_VARIATIONS_SERVICE_H_
#define COMPONENTS_VARIATIONS_SERVICE_TEST_VARIATIONS_SERVICE_H_

#include "components/variations/service/variations_service.h"

class PrefService;

namespace variations {

class TestVariationsService : public VariationsService {
 public:
  explicit TestVariationsService(PrefService* prefs);
  ~TestVariationsService() override;

  TestVariationsService(const TestVariationsService&) = delete;
  TestVariationsService& operator=(const TestVariationsService&) = delete;

  // Register Variations related prefs in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_TEST_VARIATIONS_SERVICE_H_
