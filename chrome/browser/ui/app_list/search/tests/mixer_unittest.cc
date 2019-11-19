// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/mixer.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeAppListModelUpdater;

namespace app_list {
namespace test {

using ResultType = ash::AppListSearchResultType;

// Maximum number of results to show in each mixer group.
const size_t kMaxAppsGroupResults = 4;
const size_t kMaxOmniboxResults = 4;
const size_t kMaxPlaystoreResults = 2;

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult(const std::string& id, double relevance)
      : instance_id_(instantiation_count++) {
    set_id(id);
    SetTitle(base::UTF8ToUTF16(id));
    set_relevance(relevance);
  }
  ~TestSearchResult() override {}

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
  void InvokeAction(int action_index, int event_flags) override {}
  ash::SearchResultType GetSearchResultType() const override {
    return ash::SEARCH_RESULT_TYPE_BOUNDARY;
  }

  // For reference equality testing. (Addresses cannot be used to test reference
  // equality because it is possible that an object will be allocated at the
  // same address as a previously deleted one.)
  static int GetInstanceId(ChromeSearchResult* result) {
    return static_cast<const TestSearchResult*>(result)->instance_id_;
  }

 private:
  static int instantiation_count;

  int instance_id_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchResult);
};
int TestSearchResult::instantiation_count = 0;

class TestSearchProvider : public SearchProvider {
 public:
  TestSearchProvider(const std::string& prefix, ResultType result_type)
      : prefix_(prefix),
        count_(0),
        bad_relevance_range_(false),
        small_relevance_range_(false),
        last_result_has_display_index_(false),
        display_type_(ash::SearchResultDisplayType::kList),
        result_type_(result_type) {}
  ~TestSearchProvider() override {}

  // SearchProvider overrides:
  void Start(const base::string16& query) override {
    ClearResults();
    for (size_t i = 0; i < count_; ++i) {
      const std::string id =
          base::StringPrintf("%s%d", prefix_.c_str(), static_cast<int>(i));
      double relevance = 1.0 - i / 10.0;
      // If bad_relevance_range_, change the relevances to give results outside
      // of the canonical [0.0, 1.0] range.
      if (bad_relevance_range_)
        relevance = 10.0 - i * 10;
      // If |small_relevance_range_|, keep the relevances in the same order, but
      // make the differences very small: 0.5, 0.499, 0.498, ...
      if (small_relevance_range_)
        relevance = 0.5 - i / 100.0;
      TestSearchResult* result = new TestSearchResult(id, relevance);
      result->SetDisplayType(display_type_);
      result->SetResultType(result_type_);

      if (last_result_has_display_index_ && i == count_ - 1)
        result->SetDisplayIndex(ash::SearchResultDisplayIndex::kFirstIndex);

      Add(std::unique_ptr<ChromeSearchResult>(result));
    }
  }

  void set_prefix(const std::string& prefix) { prefix_ = prefix; }
  void SetDisplayType(ChromeSearchResult::DisplayType display_type) {
    display_type_ = display_type;
  }
  void set_count(size_t count) { count_ = count; }
  void set_bad_relevance_range() { bad_relevance_range_ = true; }
  void set_small_relevance_range() { small_relevance_range_ = true; }
  void set_last_result_has_display_index() {
    last_result_has_display_index_ = true;
  }

 private:
  std::string prefix_;
  size_t count_;
  bool bad_relevance_range_;
  bool small_relevance_range_;
  bool last_result_has_display_index_;
  ChromeSearchResult::DisplayType display_type_;
  ResultType result_type_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchProvider);
};

class MixerTest : public testing::Test {
 public:
  MixerTest() {}
  ~MixerTest() override {}

  // testing::Test overrides:
  void SetUp() override {
    model_updater_ = std::make_unique<FakeAppListModelUpdater>();

    providers_.push_back(
        std::make_unique<TestSearchProvider>("app", ResultType::kInternalApp));
    providers_.push_back(
        std::make_unique<TestSearchProvider>("omnibox", ResultType::kOmnibox));
    providers_.push_back(std::make_unique<TestSearchProvider>(
        "playstore", ResultType::kPlayStoreApp));
  }

  void CreateMixer() {
    mixer_ = std::make_unique<Mixer>(model_updater_.get());

    // TODO(warx): when fullscreen app list is default enabled, modify this test
    // to test answer card/apps group having relevance boost.
    size_t apps_group_id = mixer_->AddGroup(kMaxAppsGroupResults, 1.0, 0.0);
    size_t omnibox_group_id = mixer_->AddGroup(kMaxOmniboxResults, 1.0, 0.0);
    size_t playstore_group_id =
        mixer_->AddGroup(kMaxPlaystoreResults, 0.5, 0.0);

    mixer_->AddProviderToGroup(apps_group_id, providers_[0].get());
    mixer_->AddProviderToGroup(omnibox_group_id, providers_[1].get());
    mixer_->AddProviderToGroup(playstore_group_id, providers_[2].get());
  }

  void RunQuery() {
    const base::string16 query;

    for (size_t i = 0; i < providers_.size(); ++i)
      providers_[i]->Start(query);

    mixer_->MixAndPublish(ash::AppListConfig::instance().max_search_results(),
                          base::string16());
  }

  std::string GetResults() const {
    auto& results = model_updater_->search_results();
    std::string result;
    for (size_t i = 0; i < results.size(); ++i) {
      if (!result.empty())
        result += ',';

      result += base::UTF16ToUTF8(results[i]->title());
    }

    return result;
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  Mixer* mixer() { return mixer_.get(); }
  TestSearchProvider* app_provider() { return providers_[0].get(); }
  TestSearchProvider* omnibox_provider() { return providers_[1].get(); }
  TestSearchProvider* playstore_provider() { return providers_[2].get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<Mixer> mixer_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;

  std::vector<std::unique_ptr<TestSearchProvider>> providers_;

  DISALLOW_COPY_AND_ASSIGN(MixerTest);
};

TEST_F(MixerTest, Basic) {
  CreateMixer();

  // Note: Some cases in |expected| have vastly more results than others, due to
  // the "at least 6" mechanism. If it gets at least 6 results from all
  // providers, it stops at 6. If not, it fetches potentially many more results
  // from all providers. Not ideal, but currently by design.
  struct TestCase {
    const size_t app_results;
    const size_t omnibox_results;
    const size_t playstore_results;
    const char* expected;
  } kTestCases[] = {
      {0, 0, 0, ""},
      {10, 0, 0, "app0,app1,app2,app3,app4,app5,app6,app7,app8,app9"},
      {0, 0, 10,
       "playstore0,playstore1,playstore2,playstore3,playstore4,playstore5,"
       "playstore6,playstore7,playstore8,playstore9"},
      {4, 6, 0, "app0,omnibox0,app1,omnibox1,app2,omnibox2,app3,omnibox3"},
      {4, 6, 2,
       "app0,omnibox0,app1,omnibox1,app2,omnibox2,app3,omnibox3,playstore0,"
       "playstore1"},
      {10, 10, 10,
       "app0,omnibox0,app1,omnibox1,app2,omnibox2,app3,omnibox3,playstore0,"
       "playstore1"},
      {0, 10, 0,
       "omnibox0,omnibox1,omnibox2,omnibox3,omnibox4,omnibox5,omnibox6,"
       "omnibox7,omnibox8,omnibox9"},
      {0, 10, 1,
       "omnibox0,omnibox1,omnibox2,omnibox3,playstore0,omnibox4,omnibox5,"
       "omnibox6,omnibox7,omnibox8,omnibox9"},
      {0, 10, 2, "omnibox0,omnibox1,omnibox2,omnibox3,playstore0,playstore1"},
      {1, 10, 0,
       "app0,omnibox0,omnibox1,omnibox2,omnibox3,omnibox4,omnibox5,omnibox6,"
       "omnibox7,omnibox8,omnibox9"},
      {2, 10, 0, "app0,omnibox0,app1,omnibox1,omnibox2,omnibox3"},
      {2, 10, 1, "app0,omnibox0,app1,omnibox1,omnibox2,omnibox3,playstore0"},
      {2, 10, 2,
       "app0,omnibox0,app1,omnibox1,omnibox2,omnibox3,playstore0,playstore1"},
      {2, 0, 2, "app0,app1,playstore0,playstore1"},
      {0, 0, 0, ""}};

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    app_provider()->set_count(kTestCases[i].app_results);
    omnibox_provider()->set_count(kTestCases[i].omnibox_results);
    playstore_provider()->set_count(kTestCases[i].playstore_results);
    RunQuery();

    EXPECT_EQ(kTestCases[i].expected, GetResults()) << "Case " << i;
  }
}

// Tests that results with display index defined, will be shown in the final
// results.
TEST_F(MixerTest, ResultsWithDisplayIndex) {
  CreateMixer();

  // If the last result has no display index defined, it will not shown in the
  // final results.
  app_provider()->set_count(6);
  omnibox_provider()->set_count(6);
  playstore_provider()->set_count(6);
  RunQuery();

  EXPECT_EQ(
      "app0,omnibox0,app1,omnibox1,app2,omnibox2,app3,omnibox3,playstore0,"
      "playstore1",
      GetResults());

  // If the last result has display index defined, it will be in the final
  // results.
  app_provider()->set_last_result_has_display_index();
  RunQuery();

  EXPECT_EQ(
      "app5,app0,omnibox0,app1,omnibox1,app2,omnibox2,omnibox3,playstore0,"
      "playstore1",
      GetResults());
}

TEST_F(MixerTest, RemoveDuplicates) {
  CreateMixer();

  const std::string dup = "dup";

  // This gives "dup0,dup1,dup2".
  app_provider()->set_prefix(dup);
  app_provider()->set_count(3);

  // This gives "dup0,dup1".
  omnibox_provider()->set_prefix(dup);
  omnibox_provider()->set_count(2);

  // This gives "dup0".
  playstore_provider()->set_prefix(dup);
  playstore_provider()->set_count(1);

  RunQuery();

  // Only three results with unique id are kept.
  EXPECT_EQ("dup0,dup1,dup2", GetResults());
}

}  // namespace test
}  // namespace app_list
