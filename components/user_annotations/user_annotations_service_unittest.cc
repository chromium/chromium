// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/user_annotations/user_annotations_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_annotations {

class UserAnnotationsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    service_ = std::make_unique<UserAnnotationsService>();
  }

  UserAnnotationsService* service() { return service_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<UserAnnotationsService> service_;
};

TEST_F(UserAnnotationsServiceTest, RetrieveAllEntriesNoDB) {
  base::test::TestFuture<std::vector<Entry>> test_future;
  service()->RetrieveAllEntries(test_future.GetCallback());

  auto entries = test_future.Take();
  EXPECT_TRUE(entries.empty());
}

}  // namespace user_annotations
