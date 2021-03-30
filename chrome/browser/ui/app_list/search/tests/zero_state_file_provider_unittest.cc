// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/zero_state_file_provider.h"

#include <string>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

using base::test::ScopedFeatureList;
using ::file_manager::file_tasks::FileTasksObserver;
using ::testing::UnorderedElementsAre;

MATCHER_P(Title, title, "") {
  return base::UTF16ToUTF8(arg->title()) == title;
}

}  // namespace

class ZeroStateFileProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    provider_ = std::make_unique<ZeroStateFileProvider>(profile_.get());
    app_list_color_provider_ =
        std::make_unique<ash::TestAppListColorProvider>();

    Wait();
  }

  void TearDown() override { app_list_color_provider_.reset(); }

  base::FilePath Path(const std::string& filename) {
    return profile_->GetPath().AppendASCII(filename);
  }

  void WriteFile(const std::string& filename) {
    CHECK(base::WriteFile(Path(filename), "abcd"));
    CHECK(base::PathExists(Path(filename)));
    Wait();
  }

  FileTasksObserver::FileOpenEvent OpenEvent(const std::string& filename) {
    FileTasksObserver::FileOpenEvent e;
    e.path = Path(filename);
    e.open_type = FileTasksObserver::OpenType::kOpen;
    return e;
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<Profile> profile_;
  std::unique_ptr<ZeroStateFileProvider> provider_;
  std::unique_ptr<ash::TestAppListColorProvider> app_list_color_provider_;
};

TEST_F(ZeroStateFileProviderTest, NoResultsWithQuery) {
  provider_->Start(u"query");
  Wait();
  EXPECT_TRUE(provider_->results().empty());
}

TEST_F(ZeroStateFileProviderTest, ResultsProvided) {
  // Disable flag.
  scoped_feature_list_.InitWithFeatures(
      {}, {app_list_features::kEnableSuggestedFiles});

  WriteFile("exists_1.txt");
  WriteFile("exists_2.png");
  WriteFile("exists_3.pdf");

  provider_->OnFilesOpened(
      {OpenEvent("exists_1.txt"), OpenEvent("exists_2.png")});
  provider_->OnFilesOpened({OpenEvent("nonexistant.txt")});

  provider_->Start(std::u16string());
  Wait();

  EXPECT_THAT(
      provider_->results(),
      UnorderedElementsAre(Title("exists_1.txt"), Title("exists_2.png")));
}

TEST_F(ZeroStateFileProviderTest, ResultsProvidedWithChips) {
  // Enable flag - with flag enabled, we expect to receive the chip
  // results for each file as well, so each file should be listed twice.
  scoped_feature_list_.InitWithFeatures(
      {app_list_features::kEnableSuggestedFiles}, {});

  WriteFile("exists_1.txt");
  WriteFile("exists_2.png");
  WriteFile("exists_3.pdf");

  provider_->OnFilesOpened(
      {OpenEvent("exists_1.txt"), OpenEvent("exists_2.png")});
  provider_->OnFilesOpened({OpenEvent("nonexistant.txt")});

  provider_->Start(std::u16string());
  Wait();

  EXPECT_THAT(
      provider_->results(),
      UnorderedElementsAre(Title("exists_1.txt"), Title("exists_2.png"),
                           Title("exists_1.txt"), Title("exists_2.png")));
}

}  // namespace app_list
