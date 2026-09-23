// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/content/browser/content_language_detection_driver.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/language_detection/core/browser/language_detection_model_provider.h"
#include "components/language_detection/testing/language_detection_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language_detection {

class ContentLanguageDetectionDriverTest : public testing::Test {
 protected:
  mojom::LanguageDetectionModelStatus GetStatus(
      ContentLanguageDetectionDriver& driver) {
    base::test::TestFuture<mojom::LanguageDetectionModelStatus> future;
    driver.GetLanguageDetectionModelStatus(future.GetCallback());
    return future.Get();
  }

  base::test::TaskEnvironment environment_;
};

TEST_F(ContentLanguageDetectionDriverTest, NullProviderIsNotAvailable) {
  ContentLanguageDetectionDriver driver(nullptr);
  EXPECT_EQ(GetStatus(driver),
            mojom::LanguageDetectionModelStatus::kNotAvailable);
}

TEST_F(ContentLanguageDetectionDriverTest, UnsetProviderIsAfterDownload) {
  LanguageDetectionModelProvider provider(
      environment_.GetMainThreadTaskRunner());
  ContentLanguageDetectionDriver driver(&provider);
  EXPECT_EQ(GetStatus(driver),
            mojom::LanguageDetectionModelStatus::kAfterDownload);
}

TEST_F(ContentLanguageDetectionDriverTest,
       UnloadedProviderIsNotAvailable_WebViewAndContentShellFallback) {
  LanguageDetectionModelProvider provider(
      environment_.GetMainThreadTaskRunner());

  // CreateLanguageDetectionModelProvider() in content_browser_client.cc calls
  // UnloadModelFile() immediately when OptimizationGuide is absent (e.g.
  // content_shell, Android WebView).
  provider.UnloadModelFile();
  EXPECT_TRUE(provider.HasModelEverBeenSet());
  EXPECT_FALSE(provider.HasValidModelFile());

  ContentLanguageDetectionDriver driver(&provider);
  EXPECT_EQ(GetStatus(driver),
            mojom::LanguageDetectionModelStatus::kNotAvailable);
}

TEST_F(ContentLanguageDetectionDriverTest,
       InvalidModelFileTransitionsToNotAvailable) {
  LanguageDetectionModelProvider provider(
      environment_.GetMainThreadTaskRunner());
  base::test::TestFuture<base::File> load_future;
  provider.GetLanguageDetectionModelFile(load_future.GetCallback());
  provider.ReplaceModelFile(
      base::FilePath(FILE_PATH_LITERAL("nonexistent_invalid_model.tflite")));
  EXPECT_FALSE(load_future.Take().IsValid());

  ContentLanguageDetectionDriver driver(&provider);
  EXPECT_TRUE(provider.HasModelEverBeenSet());
  EXPECT_FALSE(provider.HasValidModelFile());
  EXPECT_EQ(GetStatus(driver),
            mojom::LanguageDetectionModelStatus::kNotAvailable);
}

TEST_F(ContentLanguageDetectionDriverTest, ValidModelAndSubsequentUnload) {
  LanguageDetectionModelProvider provider(
      environment_.GetMainThreadTaskRunner());
  base::test::TestFuture<base::File> load_future;
  provider.GetLanguageDetectionModelFile(load_future.GetCallback());
  provider.ReplaceModelFile(GetValidModelFilePath());
  EXPECT_TRUE(load_future.Take().IsValid());

  ContentLanguageDetectionDriver driver(&provider);
  EXPECT_EQ(GetStatus(driver), mojom::LanguageDetectionModelStatus::kReadily);

  provider.UnloadModelFile();
  EXPECT_EQ(GetStatus(driver),
            mojom::LanguageDetectionModelStatus::kNotAvailable);
}

}  // namespace language_detection
