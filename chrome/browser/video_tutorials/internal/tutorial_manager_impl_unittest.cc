// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_manager_impl.h"

#include <map>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/video_tutorials/internal/tutorial_store.h"
#include "chrome/browser/video_tutorials/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;

namespace video_tutorials {
namespace {

proto::VideoTutorialGroups CreateSampleGroups(
    const std::vector<std::string>& locales,
    const std::vector<FeatureType>& features) {
  proto::VideoTutorialGroups groups;
  for (const auto& locale : locales) {
    proto::VideoTutorialGroup* group = groups.add_tutorial_groups();
    group->set_language(locale);
    for (FeatureType feature : features) {
      proto::VideoTutorial* tutorial = group->add_tutorials();
      tutorial->set_feature(FromFeatureType(feature));
    }
  }

  return groups;
}

class TestStore : public TutorialStore {
 public:
  TestStore() : TutorialStore(nullptr) {}
  ~TestStore() override = default;

  void InitAndLoad(LoadCallback callback) override {
    std::move(callback).Run(
        true, std::make_unique<proto::VideoTutorialGroups>(groups_));
  }

  void Update(const proto::VideoTutorialGroups& entry,
              UpdateCallback callback) override {
    groups_ = entry;
    std::move(callback).Run(true);
  }

  void Update(const proto::VideoTutorialGroups groups) {
    Update(groups, base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
    base::RunLoop().RunUntilIdle();
  }

 private:
  proto::VideoTutorialGroups groups_;
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

  // Run Initialize call from manager_ and waits for callback completion.
  void Initialize() {
    base::RunLoop loop;
    manager()->Initialize(base::BindOnce(&TutorialManagerTest::OnInitialize,
                                         base::Unretained(this),
                                         loop.QuitClosure()));
    loop.Run();
  }

  void OnInitialize(base::RepeatingClosure closure, bool success) {
    std::move(closure).Run();
  }

  // Run GetTutorials call from manager_ and caches the results in
  // last_results_.
  void GetTutorials() {
    base::RunLoop loop;
    last_results_.clear();
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
    last_get_tutorial_result_ = absl::nullopt;
    manager()->GetTutorial(
        feature_type,
        base::BindOnce(&TutorialManagerTest::OnGetTutorial,
                       base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  void OnGetTutorial(base::RepeatingClosure closure,
                     absl::optional<Tutorial> tutorial) {
    last_get_tutorial_result_ = tutorial;
    std::move(closure).Run();
  }

  void SaveGroups(std::unique_ptr<proto::VideoTutorialGroups> groups) {
    manager()->SaveGroups(std::move(groups));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  TutorialManager* manager() { return manager_.get(); }
  TestStore* tutorial_store() { return tutorial_store_; }
  std::vector<Tutorial> last_results() { return last_results_; }
  absl::optional<Tutorial> last_get_tutorial_result() {
    return last_get_tutorial_result_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TutorialManager> manager_;
  raw_ptr<TestStore> tutorial_store_;
  std::vector<Tutorial> last_results_;
  absl::optional<Tutorial> last_get_tutorial_result_;
};

TEST_F(TutorialManagerTest, InitAndGetTutorials) {
  std::vector<FeatureType> features(
      {FeatureType::kDownload, FeatureType::kSearch});
  auto groups = CreateSampleGroups({"hi", "kn"}, features);
  auto* tutorial = groups.mutable_tutorial_groups(1)->add_tutorials();
  tutorial->set_feature(FromFeatureType(FeatureType::kVoiceSearch));

  auto tutorial_store = std::make_unique<StrictMock<TestStore>>();
  tutorial_store->Update(groups);
  CreateTutorialManager(std::move(tutorial_store));
  Initialize();

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
  tutorial_store->Update(groups);
  CreateTutorialManager(std::move(tutorial_store));
  Initialize();

  auto languages = manager()->GetSupportedLanguages();
  EXPECT_EQ(languages.size(), 2u);
  manager()->SetPreferredLocale("hi");
  GetTutorials();
  EXPECT_EQ(last_results().size(), 2u);
}

TEST_F(TutorialManagerTest, SaveNewData) {
  std::vector<FeatureType> features(
      {FeatureType::kDownload, FeatureType::kSearch, FeatureType::kSummary});
  auto groups = CreateSampleGroups({"hi", "kn"}, features);
  auto tutorial_store = std::make_unique<StrictMock<TestStore>>();
  tutorial_store->Update(groups);
  CreateTutorialManager(std::move(tutorial_store));
  Initialize();

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
  auto new_groups = CreateSampleGroups({"hi", "tl", "ar"}, features);
  SaveGroups(std::make_unique<proto::VideoTutorialGroups>(new_groups));

  languages = manager()->GetSupportedLanguages();
  EXPECT_EQ(languages.size(), 2u);

  GetTutorials();
  EXPECT_EQ(last_results().size(), 2u);
}

}  // namespace

}  // namespace video_tutorials
