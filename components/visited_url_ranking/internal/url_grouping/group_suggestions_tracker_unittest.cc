// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_tracker.h"

#include "base/hash/hash.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace visited_url_ranking {

namespace {

// Helper to create InputContext for tests.
scoped_refptr<segmentation_platform::InputContext> CreateTestInputContext(
    int tab_id,
    const std::string& url_spec) {
  auto input_context =
      base::MakeRefCounted<segmentation_platform::InputContext>();
  input_context->metadata_args.emplace(
      "tab_id", segmentation_platform::processing::ProcessedValue(
                    static_cast<float>(tab_id)));
  if (!url_spec.empty()) {
    input_context->metadata_args.emplace(
        "url",
        segmentation_platform::processing::ProcessedValue(GURL(url_spec)));
  }
  return input_context;
}

// Helper to create a list of InputContexts for tests.
std::vector<scoped_refptr<segmentation_platform::InputContext>>
CreateTestInputs(
    const std::vector<std::pair<int, std::string>>& tab_url_pairs) {
  std::vector<scoped_refptr<segmentation_platform::InputContext>> inputs;
  for (const auto& pair : tab_url_pairs) {
    inputs.push_back(CreateTestInputContext(pair.first, pair.second));
  }
  return inputs;
}

}  // namespace

class GroupSuggestionsTrackerTest : public testing::Test {
 public:
  GroupSuggestionsTrackerTest() = default;
  ~GroupSuggestionsTrackerTest() override = default;

  void SetUp() override {
    Test::SetUp();
    auto* registry = pref_service_.registry();
    GroupSuggestionsTracker::RegisterProfilePrefs(registry);
    tracker_ = std::make_unique<GroupSuggestionsTracker>(&pref_service_);
  }

  void TearDown() override {
    tracker_.reset();
    Test::TearDown();
  }

  void VerifySuggestionsStorage(
      const std::vector<GroupSuggestion>& suggestions) {
    const auto& suggestion_list = pref_service_.GetList(
        GroupSuggestionsTracker::kGroupSuggestionsTrackerStatePref);
    ASSERT_EQ(suggestions.size(), suggestion_list.size());
    for (unsigned i = 0; i < suggestions.size(); ++i) {
      const GroupSuggestion& suggestion = suggestions[i];
      const base::Value::Dict& suggestion_dic = suggestion_list[i].GetDict();
      const base::Value::List& dic_tab_id_list =
          suggestion_dic
              .Find(GroupSuggestionsTracker::
                        kGroupSuggestionsTrackerUserTabIdsKey)
              ->GetList();
      ASSERT_EQ(dic_tab_id_list.size(), suggestion.tab_ids.size());
      for (unsigned j = 0; j < suggestion.tab_ids.size(); j++) {
        ASSERT_EQ(suggestion.tab_ids[j], dic_tab_id_list[j].GetInt());
      }
    }
  }

  // Helper to verify stored host hashes in prefs.
  void VerifyStoredHostHashes(
      const std::vector<std::set<int>>& expected_host_hashes_list) {
    const auto& suggestion_list_pref = pref_service_.GetList(
        GroupSuggestionsTracker::kGroupSuggestionsTrackerStatePref);
    ASSERT_EQ(expected_host_hashes_list.size(), suggestion_list_pref.size());

    for (size_t i = 0; i < suggestion_list_pref.size(); ++i) {
      const base::Value::Dict& suggestion_dic =
          suggestion_list_pref[i].GetDict();
      const base::Value::List* dic_host_hashes_list_ptr =
          suggestion_dic.FindList(
              GroupSuggestionsTracker::kGroupSuggestionsTrackerHostHashesKey);
      ASSERT_TRUE(dic_host_hashes_list_ptr)
          << "Host hashes key not found for suggestion " << i;

      std::set<int> actual_host_hashes;
      for (const base::Value& hash_val : *dic_host_hashes_list_ptr) {
        actual_host_hashes.insert(hash_val.GetInt());
      }
      EXPECT_EQ(actual_host_hashes, expected_host_hashes_list[i])
          << "Host hash mismatch for suggestion " << i;
    }
  }

  // Wrapper for AddSuggestion to pass inputs.
  void AddSuggestion(
      const GroupSuggestion& suggestion,
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs,
      GroupSuggestionsDelegate::UserResponse user_response) {
    tracker_->AddSuggestion(suggestion, inputs, user_response);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<GroupSuggestionsTracker> tracker_;
};

TEST_F(GroupSuggestionsTrackerTest, ShouldShowSuggestion_EmptySuggestion) {
  GroupSuggestion suggestion;
  std::vector<scoped_refptr<segmentation_platform::InputContext>> empty_inputs;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion, empty_inputs));
}

TEST_F(GroupSuggestionsTrackerTest, ShouldShowSuggestion_UnknownReason) {
  GroupSuggestion suggestion;
  suggestion.tab_ids = {1, 2, 3};
  suggestion.suggestion_reason = GroupSuggestion::SuggestionReason::kUnknown;
  std::vector<scoped_refptr<segmentation_platform::InputContext>> empty_inputs;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion, empty_inputs));
}

TEST_F(GroupSuggestionsTrackerTest, ShouldShowSuggestion_FirstTime) {
  GroupSuggestion suggestion;
  suggestion.tab_ids = {1, 2, 3};
  suggestion.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  std::vector<scoped_refptr<segmentation_platform::InputContext>> empty_inputs;
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(suggestion, empty_inputs));
}

TEST_F(GroupSuggestionsTrackerTest, ShouldShowSuggestion_OverlappingTabs) {
  std::vector<scoped_refptr<segmentation_platform::InputContext>> empty_inputs;
  std::vector<GroupSuggestion> suggestions;
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2, 3};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  tracker_->AddSuggestion(suggestion1, empty_inputs,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);
  suggestions.push_back(std::move(suggestion1));
  VerifySuggestionsStorage(suggestions);

  GroupSuggestion suggestion2;
  suggestion2.tab_ids = {3, 4, 5};
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(suggestion2, empty_inputs));

  GroupSuggestion suggestion3;
  suggestion3.tab_ids = {4, 5, 6};
  suggestion3.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(suggestion3, empty_inputs));

  tracker_->AddSuggestion(suggestion3, empty_inputs,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);
  suggestions.push_back(std::move(suggestion3));
  VerifySuggestionsStorage(suggestions);

  GroupSuggestion suggestion4;
  suggestion4.tab_ids = {1, 4, 7};
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion4, empty_inputs));
}

TEST_F(GroupSuggestionsTrackerTest,
       ShouldShowSuggestion_PersistShownSuggestions) {
  std::vector<scoped_refptr<segmentation_platform::InputContext>> empty_inputs;
  std::vector<GroupSuggestion> suggestions;
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2, 3};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  tracker_->AddSuggestion(suggestion1, empty_inputs,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);
  suggestions.push_back(std::move(suggestion1));
  VerifySuggestionsStorage(suggestions);

  GroupSuggestion suggestion2;
  suggestion2.tab_ids = {2, 3, 4};
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion2, empty_inputs));

  // Reset GroupSuggestionsTracker instance.
  tracker_.reset();
  tracker_ = std::make_unique<GroupSuggestionsTracker>(&pref_service_);
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion2, empty_inputs));
}

TEST_F(GroupSuggestionsTrackerTest, ShouldShowSuggestion_DifferentReasons) {
  std::vector<scoped_refptr<segmentation_platform::InputContext>> empty_inputs;
  std::vector<GroupSuggestion> suggestions;
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2, 3};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  tracker_->AddSuggestion(suggestion1, empty_inputs,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);
  suggestions.push_back(std::move(suggestion1));
  VerifySuggestionsStorage(suggestions);

  GroupSuggestion suggestion2;
  suggestion2.tab_ids = {1, 2, 3};
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSwitchedBetween;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion2, empty_inputs));

  GroupSuggestion suggestion3;
  suggestion3.tab_ids = {1, 2, 3};
  suggestion3.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSimilarSource;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion3, empty_inputs));
}

TEST_F(GroupSuggestionsTrackerTest,
       ShouldShowSuggestion_OverlappingTabs_SwitchedBetween) {
  std::vector<scoped_refptr<segmentation_platform::InputContext>> empty_inputs;
  std::vector<GroupSuggestion> suggestions;
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSwitchedBetween;
  tracker_->AddSuggestion(suggestion1, empty_inputs,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);
  suggestions.push_back(std::move(suggestion1));
  VerifySuggestionsStorage(suggestions);

  GroupSuggestion suggestion2;
  suggestion2.tab_ids = {1, 2};
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSwitchedBetween;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion2, empty_inputs));

  GroupSuggestion suggestion3;
  suggestion3.tab_ids = {1, 3};
  suggestion3.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSwitchedBetween;
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(suggestion3, empty_inputs));
  tracker_->AddSuggestion(suggestion3, empty_inputs,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);
  suggestions.push_back(std::move(suggestion3));
  VerifySuggestionsStorage(suggestions);

  GroupSuggestion suggestion4;
  suggestion4.tab_ids = {2, 3};
  suggestion4.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSwitchedBetween;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion4, empty_inputs));
}

TEST_F(GroupSuggestionsTrackerTest,
       ShouldShowSuggestion_OverlappingTabs_SimilarSource) {
  std::vector<scoped_refptr<segmentation_platform::InputContext>> empty_inputs;
  std::vector<GroupSuggestion> suggestions;
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2, 3};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSimilarSource;
  tracker_->AddSuggestion(suggestion1, empty_inputs,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);
  suggestions.push_back(std::move(suggestion1));
  VerifySuggestionsStorage(suggestions);

  GroupSuggestion suggestion2;
  suggestion2.tab_ids = {3, 4, 5};
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSimilarSource;
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(suggestion2, empty_inputs));

  tracker_->AddSuggestion(suggestion2, empty_inputs,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);
  suggestions.push_back(std::move(suggestion2));
  VerifySuggestionsStorage(suggestions);

  GroupSuggestion suggestion3;
  suggestion3.tab_ids = {4, 5, 6};
  suggestion3.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSimilarSource;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion3, empty_inputs));
}

TEST_F(GroupSuggestionsTrackerTest, AddSuggestion_Storeshosthashes) {
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2, 3};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  auto inputs1 = CreateTestInputs({{1, "https://hosta.com/path1"},
                                   {2, "https://hostb.com/path2"},
                                   {3, "https://hosta.com/path3"}});

  AddSuggestion(suggestion1, inputs1,
                GroupSuggestionsDelegate::UserResponse::kAccepted);

  std::set<int> expected_hashes1;
  expected_hashes1.insert(base::PersistentHash("hosta.com"));
  expected_hashes1.insert(base::PersistentHash("hostb.com"));
  VerifyStoredHostHashes({expected_hashes1});

  GroupSuggestion suggestion2;
  suggestion2.tab_ids = {4, 5};
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  auto inputs2 = CreateTestInputs(
      {{4, "https://hostc.com/path"}, {5, "https://hostd.com/path"}});

  AddSuggestion(suggestion2, inputs2,
                GroupSuggestionsDelegate::UserResponse::kRejected);

  std::set<int> expected_hashes2;
  expected_hashes2.insert(base::PersistentHash("hostc.com"));
  expected_hashes2.insert(base::PersistentHash("hostd.com"));
  VerifyStoredHostHashes({expected_hashes1, expected_hashes2});
}

TEST_F(GroupSuggestionsTrackerTest,
       ShouldShowSuggestion_Overlappinghosthashes) {
  // Threshold for kRecentlyOpened is 0.55
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2, 3};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  auto inputs1 = CreateTestInputs({{1, "https://hosta.com/p1"},
                                   {2, "https://hostb.com/p2"},
                                   {3, "https://hostc.com/p3"}});
  AddSuggestion(suggestion1, inputs1,
                GroupSuggestionsDelegate::UserResponse::kAccepted);

  GroupSuggestion suggestion2;  // Candidate
  suggestion2.tab_ids = {4, 5, 6};
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  auto inputs2 =
      CreateTestInputs({{4, "https://hosta.com/p4"},  // Overlaps hosta
                        {5, "https://hostb.com/p5"},  // Overlaps hostb
                        {6, "https://hostd.com/p6"}});
  // Candidate hosts: A, B, D. Stored hosts: A, B, C.
  // Overlapping hosts with stored: A, B. Count = 2.
  // Total hosts in candidate suggestion2 = 3. Ratio = 2/3 = 0.66.
  // 0.66 > 0.55 (threshold for kRecentlyOpened), so should NOT show.
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion2, inputs2));

  GroupSuggestion suggestion3;  // Candidate
  suggestion3.tab_ids = {7, 8, 9};
  suggestion3.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  auto inputs3 =
      CreateTestInputs({{7, "https://hosta.com/p7"},  // Overlaps hosta
                        {8, "https://hoste.com/p8"},
                        {9, "https://hostf.com/p9"}});
  // Candidate hosts: A, E, F. Stored hosts: A, B, C.
  // Overlapping hosts with stored: A. Count = 1.
  // Total hosts in candidate suggestion3 = 3. Ratio = 1/3 = 0.33.
  // 0.33 <= 0.55, so should show.
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(suggestion3, inputs3));
}

TEST_F(GroupSuggestionsTrackerTest,
       ShouldShowSuggestion_NoHostOverlapIfCandidateInputsEmpty) {
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2, 3};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  auto inputs1 = CreateTestInputs({{1, "https://hosta.com/p1"},
                                   {2, "https://hostb.com/p2"},
                                   {3, "https://hostc.com/p3"}});
  AddSuggestion(suggestion1, inputs1,
                GroupSuggestionsDelegate::UserResponse::kAccepted);

  GroupSuggestion suggestion2;
  suggestion2.tab_ids = {4, 5, 6};  // New tab IDs to pass tab overlap check
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  std::vector<scoped_refptr<segmentation_platform::InputContext>>
      empty_candidate_inputs;
  // If candidate inputs are empty, GethostforTab returns nullopt,
  // suggestion_hosts for candidate is empty, hosts_overlap is 0.
  // Host check passes.
  EXPECT_TRUE(
      tracker_->ShouldShowSuggestion(suggestion2, empty_candidate_inputs));
}

TEST_F(GroupSuggestionsTrackerTest,
       ShouldShowSuggestion_HostOverlapWithEmptyCandidateHostsFromInputs) {
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2, 3};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  auto inputs1 = CreateTestInputs({{1, "https://hosta.com/p1"},
                                   {2, "https://hostb.com/p2"},
                                   {3, "https://hostc.com/p3"}});
  AddSuggestion(suggestion1, inputs1,
                GroupSuggestionsDelegate::UserResponse::kAccepted);

  GroupSuggestion suggestion2;  // Candidate
  suggestion2.tab_ids = {4, 5, 6};
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  // Inputs for candidate tabs exist, but without URLs.
  auto inputs2_no_urls = CreateTestInputs({{4, ""}, {5, ""}, {6, ""}});
  // GethostforTab will return nullopt for these, candidate's suggestion_hosts
  // empty. Host overlap will be 0.
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(suggestion2, inputs2_no_urls));
}

TEST_F(GroupSuggestionsTrackerTest,
       ShouldShowSuggestion_TabOverlapDominatesHostOverlap) {
  // Threshold for kRecentlyOpened is 0.55 for both tab and host overlap.
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2, 3, 7, 8};  // 5 tabs
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  auto inputs1 = CreateTestInputs({{1, "https://hosta.com"},
                                   {2, "https://hostb.com"},
                                   {3, "https://hostc.com"},
                                   {7, "https://hostg.com"},
                                   {8, "https://hosth.com"}});
  AddSuggestion(suggestion1, inputs1,
                GroupSuggestionsDelegate::UserResponse::kAccepted);

  // Candidate 2: High tab overlap, low host overlap
  GroupSuggestion suggestion2;
  suggestion2.tab_ids = {
      1, 2, 3, 4, 5};  // 3 overlapping tabs: 1,2,3. Overlap = 3/5 = 0.6 > 0.55
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  auto inputs2 = CreateTestInputs(
      {{1, "https://hostd.com"},
       {2, "https://hoste.com"},
       {3, "https://hostf.com"},  // Different hosts for common tabs
       {4, "https://hosti.com"},
       {5, "https://hostj.com"}});
  // Hosts for candidate: D,E,F,I,J. Stored hosts: A,B,C,G,H. Overlap = 0.
  // (Passes host check) Fails due to tab overlap.
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion2, inputs2));

  // Candidate 3: Low tab overlap, high host overlap
  GroupSuggestion suggestion3;
  suggestion3.tab_ids = {10, 11, 12, 13,
                         14};  // 0 overlapping tabs. (Passes tab check)
  suggestion3.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  auto inputs3 =
      CreateTestInputs({{10, "https://hosta.com"},
                        {11, "https://hostb.com"},
                        {12, "https://hostc.com"},  // 3 overlapping hosts
                        {13, "https://hostx.com"},
                        {14, "https://hosty.com"}});
  // Hosts for candidate: A,B,C,X,Y. Stored hosts: A,B,C,G,H. Overlap A,B,C.
  // Count 3. Host overlap = 3/5 = 0.6 > 0.55. (Fails host check)
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion3, inputs3));
}

TEST_F(GroupSuggestionsTrackerTest,
       ShouldShowSuggestion_DifferentReasonThresholdsForHostOverlap) {
  // kRecentlyOpened threshold: 0.55; kSwitchedBetween threshold: 0.60
  GroupSuggestion suggestion_stored;
  suggestion_stored.tab_ids = {1, 2, 3, 4};
  suggestion_stored.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  auto inputs_stored = CreateTestInputs({{1, "https://hosta.com"},
                                         {2, "https://hostb.com"},
                                         {3, "https://hostc.com"},
                                         {4, "https://hostd.com"}});
  AddSuggestion(suggestion_stored, inputs_stored,
                GroupSuggestionsDelegate::UserResponse::kAccepted);
  // Stored hosts: A, B, C, D

  // Candidate 1: Reason kRecentlyOpened (threshold 0.55)
  GroupSuggestion candidate1;
  candidate1.tab_ids = {10, 11, 12, 13, 14};  // New tabs
  candidate1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  auto inputs_candidate1 =
      CreateTestInputs({{10, "https://hosta.com"},
                        {11, "https://hostb.com"},
                        {12, "https://hostc.com"},  // 3 overlaps
                        {13, "https://hoste.com"},
                        {14, "https://hostf.com"}});
  // Host overlap: 3/5 = 0.6.  0.6 > 0.55 -> FAIL
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(candidate1, inputs_candidate1));

  // Candidate 2: Reason kSwitchedBetween (threshold 0.60)
  GroupSuggestion candidate2;
  candidate2.tab_ids = {20, 21, 22, 23, 24};  // New tabs
  candidate2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSwitchedBetween;
  auto inputs_candidate2 =
      CreateTestInputs({{20, "https://hosta.com"},
                        {21, "https://hostb.com"},
                        {22, "https://hostc.com"},  // 3 overlaps
                        {23, "https://hoste.com"},
                        {24, "https://hostf.com"}});
  // Host overlap: 3/5 = 0.6.  0.6 <= 0.60 -> PASS (host check)
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(candidate2, inputs_candidate2));
}

}  // namespace visited_url_ranking
