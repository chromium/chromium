// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_manager_impl.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
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
    group.locale = locale;
    group.tutorials.emplace_back(Tutorial());
    group.tutorials.emplace_back(Tutorial());
    groups.emplace_back(group);
  }

  return groups;
}

std::vector<std::unique_ptr<TutorialGroup>> CreateSampleFetchData(
    const std::vector<std::string>& locales) {
  std::vector<std::unique_ptr<TutorialGroup>> groups;
  for (const auto& group : CreateSampleGroups(locales)) {
    groups.emplace_back(std::make_unique<TutorialGroup>(group));
  }

  return groups;
}

class TestStore : public Store<TutorialGroup> {
 public:
  TestStore() = default;
  ~TestStore() override = default;

  void InitAndLoadKeys(LoadKeysCallback callback) override {
    std::vector<std::string> keys;
    for (const TutorialGroup& group : groups_)
      keys.emplace_back(group.locale);

    std::move(callback).Run(true,
                            std::make_unique<std::vector<std::string>>(keys));
  }

  void LoadEntries(const std::vector<std::string>& keys,
                   LoadEntriesCallback callback) override {
    std::vector<std::unique_ptr<TutorialGroup>> entries;
    for (const TutorialGroup& group : groups_) {
      if (group.locale != locale_)
        continue;
      entries.emplace_back(std::make_unique<TutorialGroup>(group));
    }

    std::move(callback).Run(true, std::move(entries));
  }

  void Initialize(const std::string& locale,
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

  void SetUp() override {
    auto tutorial_store = std::make_unique<StrictMock<TestStore>>();
    tutorial_store_ = tutorial_store.get();
    manager_ = std::make_unique<TutorialManagerImpl>(std::move(tutorial_store),
                                                     &prefs_);
  }

  // Run GetTutorials call from manager_, compare the |expected| to the actual
  // returned tutorials.
  void GetTutorials(std::vector<Tutorial> expected) {
    base::RunLoop loop;
    manager()->GetTutorials(base::BindOnce(
        &TutorialManagerTest::OnGetTutorials, base::Unretained(this),
        loop.QuitClosure(), std::move(expected)));
    loop.Run();
  }

  void OnGetTutorials(base::RepeatingClosure closure,
                      std::vector<Tutorial> expected,
                      std::vector<Tutorial> tutorials) {
    EXPECT_TRUE(expected.size() == tutorials.size());
    std::move(closure).Run();
  }

  void Init() {
    base::RunLoop loop;
    manager()->Init(base::BindOnce(&TutorialManagerTest::OnComplete,
                                   base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  void OnComplete(base::RepeatingClosure closure, bool success) {
    std::move(closure).Run();
  }

  void SaveGroups(std::vector<std::unique_ptr<TutorialGroup>> groups) {
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

 private:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TutorialManager> manager_;
  TestStore* tutorial_store_;
};

TEST_F(TutorialManagerTest, InitAndGetTutorials) {
  auto groups = CreateSampleGroups({"hi", "kn"});
  tutorial_store()->Initialize("hi", groups);
  Init();

  auto locales = manager()->GetSupportedLocales();
  EXPECT_EQ(locales.size(), 2u);
  GetTutorials(groups[0].tutorials);
}

TEST_F(TutorialManagerTest, SaveNewData) {
  auto groups = CreateSampleGroups({"hi", "kn"});
  tutorial_store()->Initialize("hi", groups);
  Init();

  auto locales = manager()->GetSupportedLocales();
  EXPECT_EQ(locales.size(), 2u);
  GetTutorials(groups[0].tutorials);

  auto new_groups = CreateSampleFetchData({"hi", "tl", "ar"});
  SaveGroups(std::move(new_groups));
}

}  // namespace

}  // namespace video_tutorials
