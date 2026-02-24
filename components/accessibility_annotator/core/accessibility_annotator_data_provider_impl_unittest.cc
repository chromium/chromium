// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_data_provider_impl.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

using ::base::test::RunClosure;
using ::testing::IsEmpty;

class AccessibilityAnnotatorDataProviderImplTest : public testing::Test {
 public:
  AccessibilityAnnotatorDataProviderImplTest() = default;
  ~AccessibilityAnnotatorDataProviderImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  AccessibilityAnnotatorDataProviderImpl provider_;
};

TEST_F(AccessibilityAnnotatorDataProviderImplTest, GetEntitiesReturnsEmpty) {
  base::RunLoop run_loop;
  base::MockOnceCallback<void(std::vector<Entity>)> cb;
  EXPECT_CALL(cb, Run(IsEmpty())).WillOnce(RunClosure(run_loop.QuitClosure()));
  provider_.GetEntities(/*types=*/{}, cb.Get());
  run_loop.Run();
}

}  // namespace accessibility_annotator
