// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_manager_impl.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/video_tutorials/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;

namespace video_tutorials {
namespace {

std::vector<TutorialGroup> CreateSampleGroups(
    const std::vector<std::string>& locales,
    const std::vector<FeatureType>& features) {
  std::vector<TutorialGroup> groups;
  for (const auto& locale : locales) {
    TutorialGroup group;
    group.language = locale;
    for (FeatureType feature : features) {
      group.tutorials.emplace_back(
          Tutorial(feature, "", "", "", "", "", "", "", 10));
    }

    groups.emplace_back(group);
  }

  return groups;
}

std::unique_ptr<std::vector<TutorialGroup>> CreateSampleFetchData(
    const std::vector<std::string>& locales,
    const std::vector<FeatureType>& features) {
  auto groups = std::make_unique<std::vector<TutorialGroup>>();
  for (const auto& group : CreateSampleGroups(locales, features)) {
    groups->emplace_back(group);
  }

  return groups;
}

class TestStore : public Store<TutorialGroup> {
 public:
  TestStore() = default;
  ~TestStore() override = default;

  void Initialize(SuccessCallback callback) override {
    std::move(callback).Run(true);
  }

  void LoadEntries(const std::vector<std::string>& keys,
                   LoadEntriesCallback callback) override {
    auto entries = std::make_unique<std::vector<TutorialGroup>>();
    for (const TutorialGroup& group : groups_) {
      if (keys.empty()) {
        entries->emplace_back(group);
      } else {
        for (auto& key : keys) {
          if (key == group.language) {
            entries->emplace_back(group);
          }
        }
      }
    }

    std::move(callback).Run(true, std::move(entries));
  }

  void InitStoreData(const std::string& locale,
                     const std::vector<TutorialGroup>& groups) {
    locale_ = locale;
    groups_ = groups;
  }

  void UpdateAll(
      const std::vector<std::pair<std::string, TutorialGroup>>& key_entry_pairs,
      const std::vector<std::string>& keys_to_delete,
      UpdateCallback callback) override {
    groups_.clear();
    for (auto pair : key_entry_pairs)
      groups_.emplace_back(pair.second);

    std::move(callback).Run(true);
  }

 private:
  std::string locale_;
  std::vector<TutorialGroup> groups_;
};

class TutorialManagerTest : public testing::Test {
 public:
  TutorialManagerTest() = default;
  ~TutorialManagerTest() override = default;

  TutorialManagerTest(const TutorialManagerTest& other) = delete;
  TutorialManagerTest& operator=(const TutorialManagerTest& other) = delete;

  void SetUp() override { video_tutorials::RegisterPrefs(prefs_.registry()); }

  void CreateTutorialManager(std::unique_ptr<TestStore> tutorial_store) {
    tutorial_store_ = tutorial_store.get();
    manager_ = std::make_unique<TutorialManagerImpl>(std::move(tutorial_store),
                                                     &prefs_);
  }

  // Run GetTutorials call from manager_, compare the |expected| to the actual
  // returned tutorials.
  void GetTutorials() {
    base::RunLoop loop;
    manager()->GetTutorials(base::BindOnce(&TutorialManagerTest::OnGetTutorials,
                                           base::Unretained(this),
                                           loop.QuitClosure()));
    loop.Run();
  }

  void OnGetTutorials(base::RepeatingClosure closure,
                      std::vector<Tutorial> tutorials) {
    last_results_ = tutorials;
    std::move(closure).Run();
  }

  void GetTutorial(FeatureType feature_type) {
    base::RunLoop loop;
    manager()->GetTutorial(
        feature_type,
        base::BindOnce(&TutorialManagerTest::OnGetTutorial,
                       base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  void OnGetTutorial(base::RepeatingClosure closure,
                     base::Optional<Tutorial> tutorial) {
    last_get_tutorial_result_ = tutorial;
    std::move(closure).Run();
  }

  void OnComplete(base::RepeatingClosure closure, bool success) {
    std::move(closure).Run();
  }

  void SaveGroups(std::unique_ptr<std::vector<TutorialGroup>> groups) {
    manager()->SaveGroups(std::move(groups));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  TutorialManager* manager() { return manager_.get(); }
  TestStore* tutorial_store() { return tutorial_store_; }
  std::vector<Tutorial> last_results() { return last_results_; }
  base::Optional<Tutorial> last_get_tutorial_result() {
    return last_get_tutorial_result_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TutorialManager> manager_;
  TestStore* tutorial_store_;
  std::vector<Tutorial> last_results_;
  base::Optional<Tutorial> last_get_tutorial_result_;
};

TEST_F(TutorialManagerTest, InitAndGetTutorials) {
  std::vector<FeatureType> features(
      {FeatureType::kDownload, FeatureType::kSearch});
  auto groups = CreateSampleGroups({"hi", "kn"}, features);
  groups[1].tutorials.emplace_back(
      Tutorial(FeatureType::kVoiceSearch, "", "", "", "", "", "", "", 10));
  auto tutorial_store = std::make_unique<StrictMock<TestStore>>();
  tutorial_store->InitStoreData("hi", groups);
  CreateTutorialManager(std::move(tutorial_store));

  auto languages = manager()->GetSupportedLanguages();
  EXPECT_EQ(languages.size(), 2u);
  manager()->SetPreferredLocale("hi");
  GetTutorials();
  EXPECT_EQ(last_results().size(), 2u);

  GetTutorial(FeatureType::kSearch);
  EXPECT_EQ(FeatureType::kSearch, last_get_tutorial_result()->feature);

  languages = manager()->GetAvailableLanguagesForTutorial(FeatureType::kSearch);
  EXPECT_EQ(languages.size(), 2u);
  languages =
      manager()->GetAvailableLanguagesForTutorial(FeatureType::kChromeIntro);
  EXPECT_EQ(languages.size(), 0u);
  languages =
      manager()->GetAvailableLanguagesForTutorial(FeatureType::kVoiceSearch);
  EXPECT_EQ(languages.size(), 1u);
}

TEST_F(TutorialManagerTest, InitAndGetTutorialsWithSummary) {
  std::vector<FeatureType> features(
      {FeatureType::kDownload, FeatureType::kSearch, FeatureType::kSummary});
  auto groups = CreateSampleGroups({"hi", "kn"}, features);
  auto tutorial_store = std::make_unique<StrictMock<TestStore>>();
  tutorial_store->InitStoreData("hi", groups);
  CreateTutorialManager(std::move(tutorial_store));

  auto languages = manager()->GetSupportedLanguages();
  EXPECT_EQ(languages.size(), 2u);
  manager()->SetPreferredLocale("hi");
  GetTutorials();
  EXPECT_EQ(last_results().size(), 2u);
}

TEST_F(TutorialManagerTest, GetSingleTutorialBeforeGetTutorialsCall) {
  std::vector<FeatureType> features(
      {FeatureType::kDownload, FeatureType::kSearch, FeatureType::kSummary});
  auto groups = CreateSampleGroups({"hi", "kn"}, features);
  auto tutorial_store = std::make_unique<StrictMock<TestStore>>();
  tutorial_store->InitStoreData("hi", groups);
  CreateTutorialManager(std::move(tutorial_store));

  auto languages = manager()->GetSupportedLanguages();
  EXPECT_EQ(languages.size(), 2u);
  manager()->SetPreferredLocale("hi");
  GetTutorial(FeatureType::kSummary);
  EXPECT_TRUE(last_get_tutorial_result().has_value());
  EXPECT_EQ(FeatureType::kSummary, last_get_tutorial_result()->feature);

  GetTutorial(FeatureType::kSearch);
  EXPECT_EQ(FeatureType::kSearch, last_get_tutorial_result()->feature);
  GetTutorial(FeatureType::kVoiceSearch);
  EXPECT_FALSE(last_get_tutorial_result().has_value());
}

TEST_F(TutorialManagerTest, SaveNewData) {
  std::vector<FeatureType> features(
      {FeatureType::kDownload, FeatureType::kSearch, FeatureType::kSummary});
  auto groups = CreateSampleGroups({"hi", "kn"}, features);
  auto tutorial_store = std::make_unique<StrictMock<TestStore>>();
  tutorial_store->InitStoreData("hi", groups);
  CreateTutorialManager(std::move(tutorial_store));

  manager()->SetPreferredLocale("hi");
  auto languages = manager()->GetSupportedLanguages();
  EXPECT_EQ(languages.size(), 2u);
  GetTutorials();
  EXPECT_EQ(last_results().size(), 2u);
  languages =
      manager()->GetAvailableLanguagesForTutorial(FeatureType::kDownload);
  EXPECT_EQ(languages.size(), 2u);

  // New fetch data.
  features = std::vector<FeatureType>(
      {FeatureType::kChromeIntro, FeatureType::kDownload, FeatureType::kSearch,
       FeatureType::kVoiceSearch});
  auto new_groups = CreateSampleFetchData({"hi", "tl", "ar"}, features);
  auto new_group = new_groups->at(0);
  SaveGroups(std::move(new_groups));
  manager()->SetPreferredLocale("ar");
  GetTutorials();
  EXPECT_EQ(last_results().size(), 4u);

  languages =
      manager()->GetAvailableLanguagesForTutorial(FeatureType::kDownload);
  EXPECT_EQ(languages.size(), 3u);

  // New fetch data with summary.
  features = std::vector<FeatureType>(
      {FeatureType::kChromeIntro, FeatureType::kVoiceSearch,
       FeatureType::kSearch, FeatureType::kSummary});
  new_groups = CreateSampleFetchData({"hi", "tl", "ar"}, features);
  SaveGroups(std::move(new_groups));
  manager()->SetPreferredLocale("tl");
  GetTutorials();
  EXPECT_EQ(last_results().size(), 3u);

  languages =
      manager()->GetAvailableLanguagesForTutorial(FeatureType::kDownload);
  EXPECT_EQ(languages.size(), 0u);
}

}  // namespace

}  // namespace video_tutorials
