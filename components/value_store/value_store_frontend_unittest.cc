// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/value_store_frontend.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/value_store/test_value_store_factory.h"
#include "components/value_store/value_store_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace value_store {

class ValueStoreFrontendTest : public testing::Test {
 public:
  ValueStoreFrontendTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
    base::FilePath src_db(
        test_data_dir.AppendASCII("components/test/data/value_store"));
    db_path_ = temp_dir_.GetPath().AppendASCII("temp_db");
    base::CopyDirectory(src_db, db_path_, true);

    factory_ = new TestValueStoreFactory(db_path_);

    ResetStorage();
  }

  void TearDown() override {
    RunUntilIdle();
    storage_.reset();
  }

  // Reset the value store, reloading the DB from disk.
  void ResetStorage() {
    storage_ = std::make_unique<ValueStoreFrontend>(
        factory_, base::FilePath(FILE_PATH_LITERAL("Test dir")),
        "test_uma_name", base::SingleThreadTaskRunner::GetCurrentDefault(),
        value_store::GetValueStoreTaskRunner());
  }

  bool Get(const std::string& key, std::optional<base::Value>* output) {
    storage_->Get(key, base::BindOnce(&ValueStoreFrontendTest::GetAndWait,
                                      base::Unretained(this), output));
    RunUntilIdle();
    return output->has_value();
  }

 protected:
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void GetAndWait(std::optional<base::Value>* output,
                  std::optional<base::Value> result) {
    *output = std::move(result);
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<TestValueStoreFactory> factory_;
  std::unique_ptr<ValueStoreFrontend> storage_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
};

TEST_F(ValueStoreFrontendTest, GetExistingData) {
  std::optional<base::Value> value;
  ASSERT_FALSE(Get("key0", &value));

  // Test existing keys in the DB.
  {
    ASSERT_TRUE(Get("key1", &value));
    ASSERT_TRUE(value->is_string());
    EXPECT_EQ("value1", value->GetString());
  }

  {
    ASSERT_TRUE(Get("key2", &value));
    ASSERT_TRUE(value->is_int());
    EXPECT_EQ(2, value->GetInt());
  }
}

TEST_F(ValueStoreFrontendTest, ChangesPersistAfterReload) {
  storage_->Set("key0", base::Value(0));
  storage_->Set("key1", base::Value("new1"));
  storage_->Remove("key2");

  // Reload the DB and test our changes.
  ResetStorage();

  std::optional<base::Value> value;
  {
    ASSERT_TRUE(Get("key0", &value));
    ASSERT_TRUE(value->is_int());
    EXPECT_EQ(0, value->GetInt());
  }

  {
    ASSERT_TRUE(Get("key1", &value));
    ASSERT_TRUE(value->is_string());
    EXPECT_EQ("new1", value->GetString());
  }

  ASSERT_FALSE(Get("key2", &value));
}

}  // namespace value_store
