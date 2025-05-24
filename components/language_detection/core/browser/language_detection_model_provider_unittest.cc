// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/browser/language_detection_model_provider.h"

#include "base/test/task_environment.h"
#include "components/language_detection/testing/language_detection_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language_detection {

class LanguageDetectionModelProviderTest : public testing::Test {
 protected:
  base::test::TaskEnvironment environment_;
};

TEST_F(LanguageDetectionModelProviderTest, NoModelSet) {
  LanguageDetectionModelProvider provider(
      environment_.GetMainThreadTaskRunner());
  EXPECT_FALSE(provider.HasValidModelFile());
}

TEST_F(LanguageDetectionModelProviderTest, SetModelAndUnload) {
  base::RunLoop run_loop;
  LanguageDetectionModelProvider provider(
      environment_.GetMainThreadTaskRunner());
  ASSERT_FALSE(provider.HasValidModelFile());
  provider.GetLanguageDetectionModelFile(base::BindOnce(
      [](base::RepeatingClosure quit_closure, base::File model_file) {
        EXPECT_TRUE(model_file.IsValid());
        quit_closure.Run();
      },
      run_loop.QuitClosure()));
  provider.ReplaceModelFile(GetValidModelFilePath());
  run_loop.Run();
  ASSERT_TRUE(provider.HasValidModelFile());
  provider.UnloadModelFile();
  ASSERT_FALSE(provider.HasValidModelFile());
}

}  // namespace language_detection
