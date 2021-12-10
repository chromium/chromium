// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/private/cpp/internal_service_factory_impl.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace cros_healthd {
namespace internal {
namespace {

class CrosHealthdInternalServiceFactoryImplTest : public testing::Test {
 public:
  CrosHealthdInternalServiceFactoryImplTest() = default;
  CrosHealthdInternalServiceFactoryImplTest(
      const CrosHealthdInternalServiceFactoryImplTest&) = delete;
  CrosHealthdInternalServiceFactoryImplTest& operator=(
      const CrosHealthdInternalServiceFactoryImplTest&) = delete;

 private:
  base::test::TaskEnvironment task_environment_;
};

}  // namespace
}  // namespace internal
}  // namespace cros_healthd
}  // namespace chromeos
