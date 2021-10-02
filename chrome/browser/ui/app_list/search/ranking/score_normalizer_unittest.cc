// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/score_normalizer.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

class ScoreNormalizerTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() { return temp_dir_.GetPath().Append("proto"); }

  ScoreNormalizer::Params TestingParams() {
    ScoreNormalizer::Params params;
    params.version = 4;
    params.write_delay = base::Seconds(0);
    return params;
  }

  ScoreNormalizerProto ReadFromDisk() {
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    ScoreNormalizerProto proto;
    CHECK(proto.ParseFromString(proto_str));
    return proto;
  }

  void WriteToDisk(const ScoreNormalizerProto& proto) {
    ASSERT_TRUE(base::WriteFile(GetPath(), proto.SerializeAsString()));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  ScoreNormalizerProto* get_proto(ScoreNormalizer& normalizer) {
    return normalizer.proto_.get();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
};

// A dummy test to ensure that the score normalizer doesn't crash on
// initialization.
TEST_F(ScoreNormalizerTest, Initialize) {
  ScoreNormalizer normalizer(GetPath(), TestingParams());
  Wait();
  SUCCEED();
}

TEST_F(ScoreNormalizerTest, StateClearedOnVersionChange) {
  {
    ScoreNormalizerProto proto;
    proto.set_parameter_version(1);
    WriteToDisk(proto);
  }

  {
    ScoreNormalizer normalizer(GetPath(), TestingParams());
    Wait();
    EXPECT_EQ(get_proto(normalizer)->parameter_version(), 4);
    // TODO(crbug.com/1199206): Check that state is reset once it's added to the
    // proto.
  }
}

}  // namespace app_list
