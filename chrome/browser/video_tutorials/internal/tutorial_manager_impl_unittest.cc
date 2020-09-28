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
    const std::vector<std::string>& locales) {
  std::vector<TutorialGroup> groups;
  for (const auto& locale : locales) {
    TutorialGroup group;
    group.language.locale = locale;
    group.tutorials.emplace_back(Tutorial());
    group.tutorials.emplace_back(Tutorial());
    groups.emplace_back(group);
  }

  return groups;
}

std::unique_ptr<std::vector<TutorialGroup>> CreateSampleFetchData(
    const std::vector<std::string>& locales) {
  auto groups = std::make_unique<std::vector<TutorialGroup>>();
  for (const auto& group : CreateSampleGroups(locales)) {
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
          if (key == group.language.locale) {
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

  void OnComplete(base::RepeatingClosure closure, bool success) {
    std::move(closure).Run();
  }

  void SaveGroups(std::unique_ptr<std::vector<TutorialGroup>> groups) {
    base::RunLoop loop;
    manager()->SaveGroups(
        std::move(groups),
        base::BindOnce(&TutorialManagerTest::OnComplete, base::Unretained(this),
                       loop.QuitClosure()));
    loop.Run();
  }

 protected:
  TutorialManager* manager() { return manager_.get(); }
  TestStore* tutorial_store() { return tutorial_store_; }
  std::vector<Tutorial> last_results() { return last_results_; }

 private:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TutorialManager> manager_;
  TestStore* tutorial_store_;
  std::vector<Tutorial> last_results_;
};

TEST_F(TutorialManagerTest, InitAndGetTutorials) {
  auto groups = CreateSampleGroups({"hi", "kn"});
  auto tutorial_store = std::make_unique<StrictMock<TestStore>>();
  tutorial_store->InitStoreData("hi", groups);
  CreateTutorialManager(std::move(tutorial_store));

  auto languages = manager()->GetSupportedLanguages();
  EXPECT_EQ(languages.size(), 2u);
  GetTutorials();
  EXPECT_EQ(last_results().size(), 2u);
}

TEST_F(TutorialManagerTest, SaveNewData) {
  auto groups = CreateSampleGroups({"hi", "kn"});
  auto tutorial_store = std::make_unique<StrictMock<TestStore>>();
  tutorial_store->InitStoreData("hi", groups);
  CreateTutorialManager(std::move(tutorial_store));

  auto languages = manager()->GetSupportedLanguages();
  EXPECT_EQ(languages.size(), 2u);
  GetTutorials();
  EXPECT_EQ(last_results().size(), groups[0].tutorials.size());

  auto new_groups = CreateSampleFetchData({"hi", "tl", "ar"});
  auto new_group = new_groups->at(0);
  SaveGroups(std::move(new_groups));
  manager()->SetPreferredLocale("ar");
  GetTutorials();
  EXPECT_EQ(last_results().size(), new_group.tutorials.size());
}

}  // namespace

}  // namespace video_tutorials
