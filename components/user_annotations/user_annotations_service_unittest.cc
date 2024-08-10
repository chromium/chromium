// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
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
  base::test::TestFuture<
      std::vector<optimization_guide::proto::UserAnnotationsEntry>>
      test_future;
  service()->RetrieveAllEntries(test_future.GetCallback());

  auto entries = test_future.Take();
  EXPECT_TRUE(entries.empty());
}

TEST_F(UserAnnotationsServiceTest, RetrieveAllEntriesWithInsert) {
  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  form_field_data.set_value(u"whatever");
  autofill::FormFieldData form_field_data2;
  form_field_data2.set_name(u"nolabel");
  form_field_data2.set_value(u"value");
  autofill::FormData form_data;
  form_data.set_fields({form_field_data, form_field_data2});
  optimization_guide::proto::AXTreeUpdate ax_tree;
  service()->AddFormSubmission(ax_tree, form_data);

  base::test::TestFuture<
      std::vector<optimization_guide::proto::UserAnnotationsEntry>>
      test_future;
  service()->RetrieveAllEntries(test_future.GetCallback());

  auto entries = test_future.Take();
  EXPECT_EQ(2u, entries.size());

  EXPECT_EQ(entries[0].key(), "label");
  EXPECT_EQ(entries[0].value(), "whatever");
  EXPECT_EQ(entries[1].key(), "nolabel");
  EXPECT_EQ(entries[1].value(), "value");
}

}  // namespace user_annotations
