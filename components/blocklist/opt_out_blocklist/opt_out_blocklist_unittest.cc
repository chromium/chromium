// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/blocklist/opt_out_blocklist/opt_out_blocklist.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_delegate.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_item.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blocklist {

namespace {

const char kTestHost1[] = "testhost1.com";
const char kTestHost2[] = "testhost2.com";

// Mock class to test that OptOutBlocklist notifies the delegate with correct
// events (e.g. New host blocklisted, user blocklisted, and blocklist cleared).
class TestOptOutBlocklistDelegate : public OptOutBlocklistDelegate {
 public:
  TestOptOutBlocklistDelegate() : blocklist_cleared_time_(base::Time::Now()) {}

  // OptOutBlocklistDelegate:
  void OnNewBlocklistedHost(const std::string& host, base::Time time) override {
    blocklisted_hosts_[host] = time;
  }
  void OnUserBlocklistedStatusChange(bool blocklisted) override {
    user_blocklisted_ = blocklisted;
  }
  void OnLoadingStateChanged(bool is_loaded) override {
    blocklist_loaded_ = is_loaded;
  }
  void OnBlocklistCleared(base::Time time) override {
    blocklist_cleared_ = true;
    blocklist_cleared_time_ = time;
  }

  // Gets the set of blocklisted hosts recorded.
  const std::unordered_map<std::string, base::Time>& blocklisted_hosts() const {
    return blocklisted_hosts_;
  }

  // Gets the state of user blocklisted status.
  bool user_blocklisted() const { return user_blocklisted_; }

  // Gets the load state of blocklist.
  bool blocklist_loaded() const { return blocklist_loaded_; }

  // Gets the state of blocklisted cleared status of |this| for testing.
  bool blocklist_cleared() const { return blocklist_cleared_; }

  // Gets the event time of blocklist is as cleared.
  base::Time blocklist_cleared_time() const { return blocklist_cleared_time_; }

 private:
  // The user blocklisted status of |this| blocklist_delegate.
  bool user_blocklisted_ = false;

  // The blocklist load status of |this| blocklist_delegate.
  bool blocklist_loaded_ = false;

  // Check if the blocklist is notified as cleared on |this| blocklist_delegate.
  bool blocklist_cleared_ = false;

  // The time when blocklist is cleared.
  base::Time blocklist_cleared_time_;

  // |this| blocklist_delegate's collection of blocklisted hosts.
  std::unordered_map<std::string, base::Time> blocklisted_hosts_;
};

class TestOptOutStore : public OptOutStore {
 public:
  TestOptOutStore() = default;
  ~TestOptOutStore() override = default;

  int clear_blocklist_count() { return clear_blocklist_count_; }

  void SetBlocklistData(std::unique_ptr<BlocklistData> data) {
    data_ = std::move(data);
  }

 private:
  // OptOutStore implementation:
  void AddEntry(bool opt_out,
                const std::string& host_name,
                int type,
                base::Time now) override {}

  void LoadBlockList(std::unique_ptr<BlocklistData> blocklist_data,
                     LoadBlockListCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       data_ ? std::move(data_) : std::move(blocklist_data)));
  }

  void ClearBlockList(base::Time begin_time, base::Time end_time) override {
    ++clear_blocklist_count_;
  }

  int clear_blocklist_count_ = 0;

  std::unique_ptr<BlocklistData> data_;
};

class TestOptOutBlocklist : public OptOutBlocklist {
 public:
  TestOptOutBlocklist(std::unique_ptr<OptOutStore> opt_out_store,
                      base::Clock* clock,
                      OptOutBlocklistDelegate* blocklist_delegate)
      : OptOutBlocklist(std::move(opt_out_store), clock, blocklist_delegate) {}
  ~TestOptOutBlocklist() override = default;

  void SetSessionRule(std::unique_ptr<BlocklistData::Policy> policy) {
    session_policy_ = std::move(policy);
  }

  void SetPersistentRule(std::unique_ptr<BlocklistData::Policy> policy) {
    persistent_policy_ = std::move(policy);
  }

  void SetHostRule(std::unique_ptr<BlocklistData::Policy> policy,
                   size_t max_hosts) {
    host_policy_ = std::move(policy);
    max_hosts_ = max_hosts;
  }

  void SetTypeRule(std::unique_ptr<BlocklistData::Policy> policy) {
    type_policy_ = std::move(policy);
  }

  void SetAllowedTypes(BlocklistData::AllowedTypesAndVersions allowed_types) {
    allowed_types_ = std::move(allowed_types);
  }

 private:
  bool ShouldUseSessionPolicy(base::TimeDelta* duration,
                              size_t* history,
                              int* threshold) const override {
    if (!session_policy_) {
      return false;
    }
    *duration = session_policy_->duration;
    *history = session_policy_->history;
    *threshold = session_policy_->threshold;

    return true;
  }

  bool ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                 size_t* history,
                                 int* threshold) const override {
    if (!persistent_policy_) {
      return false;
    }
    *duration = persistent_policy_->duration;
    *history = persistent_policy_->history;
    *threshold = persistent_policy_->threshold;

    return true;
  }

  bool ShouldUseHostPolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold,
                           size_t* max_hosts) const override {
    if (!host_policy_) {
      return false;
    }
    *duration = host_policy_->duration;
    *history = host_policy_->history;
    *threshold = host_policy_->threshold;
    *max_hosts = max_hosts_;

    return true;
  }

  bool ShouldUseTypePolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold) const override {
    if (!type_policy_) {
      return false;
    }
    *duration = type_policy_->duration;
    *history = type_policy_->history;
    *threshold = type_policy_->threshold;

    return true;
  }

  BlocklistData::AllowedTypesAndVersions GetAllowedTypes() const override {
    return allowed_types_;
  }

  std::unique_ptr<BlocklistData::Policy> session_policy_;
  std::unique_ptr<BlocklistData::Policy> persistent_policy_;
  std::unique_ptr<BlocklistData::Policy> host_policy_;
  std::unique_ptr<BlocklistData::Policy> type_policy_;

  size_t max_hosts_ = 0;

  BlocklistData::AllowedTypesAndVersions allowed_types_;
};

class OptOutBlocklistTest : public testing::Test {
 public:
  OptOutBlocklistTest() = default;

  OptOutBlocklistTest(const OptOutBlocklistTest&) = delete;
  OptOutBlocklistTest& operator=(const OptOutBlocklistTest&) = delete;

  ~OptOutBlocklistTest() override = default;

  void StartTest(bool null_opt_out_store) {
    std::unique_ptr<TestOptOutStore> opt_out_store =
        null_opt_out_store ? nullptr : std::make_unique<TestOptOutStore>();
    opt_out_store_ = opt_out_store.get();

    block_list_ = std::make_unique<TestOptOutBlocklist>(
        std::move(opt_out_store), &test_clock_, &blocklist_delegate_);
    if (session_policy_) {
      block_list_->SetSessionRule(std::move(session_policy_));
    }
    if (persistent_policy_) {
      block_list_->SetPersistentRule(std::move(persistent_policy_));
    }
    if (host_policy_) {
      block_list_->SetHostRule(std::move(host_policy_), max_hosts_);
    }
    if (type_policy_) {
      block_list_->SetTypeRule(std::move(type_policy_));
    }

    block_list_->SetAllowedTypes(std::move(allowed_types_));
    block_list_->Init();

    start_ = test_clock_.Now();

    passed_reasons_ = {};
  }

  void SetSessionRule(std::unique_ptr<BlocklistData::Policy> policy) {
    session_policy_ = std::move(policy);
  }

  void SetPersistentRule(std::unique_ptr<BlocklistData::Policy> policy) {
    persistent_policy_ = std::move(policy);
  }

  void SetHostRule(std::unique_ptr<BlocklistData::Policy> policy,
                   size_t max_hosts) {
    host_policy_ = std::move(policy);
    max_hosts_ = max_hosts;
  }

  void SetTypeRule(std::unique_ptr<BlocklistData::Policy> policy) {
    type_policy_ = std::move(policy);
  }

  void SetAllowedTypes(BlocklistData::AllowedTypesAndVersions allowed_types) {
    allowed_types_ = std::move(allowed_types);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Observer to |block_list_|.
  TestOptOutBlocklistDelegate blocklist_delegate_;

  base::SimpleTestClock test_clock_;
  raw_ptr<TestOptOutStore, DanglingUntriaged> opt_out_store_;
  base::Time start_;

  std::unique_ptr<TestOptOutBlocklist> block_list_;
  std::vector<BlocklistReason> passed_reasons_;

 private:
  std::unique_ptr<BlocklistData::Policy> session_policy_;
  std::unique_ptr<BlocklistData::Policy> persistent_policy_;
  std::unique_ptr<BlocklistData::Policy> host_policy_;
  std::unique_ptr<BlocklistData::Policy> type_policy_;

  size_t max_hosts_ = 0;

  BlocklistData::AllowedTypesAndVersions allowed_types_;
};

TEST_F(OptOutBlocklistTest, HostBlockListNoStore) {
  // Tests the block list behavior when a null OptOutStore is passed in.
  auto host_policy =
      std::make_unique<BlocklistData::Policy>(base::Days(365), 4u, 2);
  SetHostRule(std::move(host_policy), 5);
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  test_clock_.Advance(base::Seconds(1));

  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));

  block_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::Seconds(1));

  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfHost,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, true, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));

  block_list_->AddEntry(kTestHost2, true, 1);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost2, true, 1);
  test_clock_.Advance(base::Seconds(1));

  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfHost,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, true, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfHost,
      block_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));

  block_list_->AddEntry(kTestHost2, false, 1);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost2, false, 1);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost2, false, 1);
  test_clock_.Advance(base::Seconds(1));

  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfHost,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, true, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));

  block_list_->ClearBlockList(start_, test_clock_.Now());

  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));
}

TEST_F(OptOutBlocklistTest, TypeBlockListWithStore) {
  // Tests the block list behavior when a non-null OptOutStore is passed in.

  auto type_policy =
      std::make_unique<BlocklistData::Policy>(base::Days(365), 4u, 2);
  SetTypeRule(std::move(type_policy));
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  allowed_types.insert({2, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(false /* null_opt_out */);

  test_clock_.Advance(base::Seconds(1));

  EXPECT_EQ(
      BlocklistReason::kBlocklistNotLoaded,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kBlocklistNotLoaded,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kBlocklistNotLoaded,
      block_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  block_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::Seconds(1));

  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfType,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfType,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 2, true, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  block_list_->AddEntry(kTestHost1, true, 2);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost1, true, 2);
  test_clock_.Advance(base::Seconds(1));

  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfType,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfType,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfType,
      block_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  block_list_->AddEntry(kTestHost1, false, 2);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost1, false, 2);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost1, false, 2);
  test_clock_.Advance(base::Seconds(1));

  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfType,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfType,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  EXPECT_EQ(0, opt_out_store_->clear_blocklist_count());
  block_list_->ClearBlockList(start_, base::Time::Now());
  EXPECT_EQ(1, opt_out_store_->clear_blocklist_count());

  EXPECT_EQ(
      BlocklistReason::kBlocklistNotLoaded,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kBlocklistNotLoaded,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kBlocklistNotLoaded,
      block_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, opt_out_store_->clear_blocklist_count());

  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));
}

TEST_F(OptOutBlocklistTest, TypeBlockListNoStore) {
  // Tests the block list behavior when a null OptOutStore is passed in.
  auto type_policy =
      std::make_unique<BlocklistData::Policy>(base::Days(365), 4u, 2);
  SetTypeRule(std::move(type_policy));
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  allowed_types.insert({2, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  test_clock_.Advance(base::Seconds(1));

  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  block_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::Seconds(1));

  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfType,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, true, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  block_list_->AddEntry(kTestHost1, true, 2);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost1, true, 2);
  test_clock_.Advance(base::Seconds(1));

  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfType,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, true, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfType,
      block_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  block_list_->AddEntry(kTestHost1, false, 2);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost1, false, 2);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost1, false, 2);
  test_clock_.Advance(base::Seconds(1));

  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfType,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, true, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  block_list_->ClearBlockList(start_, test_clock_.Now());

  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));
}

TEST_F(OptOutBlocklistTest, HostIndifferentBlocklist) {
  // Tests the block list behavior when a null OptOutStore is passed in.
  const std::string hosts[] = {
      "url_0.com",
      "url_1.com",
      "url_2.com",
      "url_3.com",
  };

  int host_indifferent_threshold = 4;

  auto persistent_policy = std::make_unique<BlocklistData::Policy>(
      base::Days(365), 4u, host_indifferent_threshold);
  SetPersistentRule(std::move(persistent_policy));
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);
  test_clock_.Advance(base::Seconds(1));

  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(hosts[0], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(hosts[1], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(hosts[2], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(hosts[3], 1, false, &passed_reasons_));

  for (int i = 0; i < host_indifferent_threshold; i++) {
    block_list_->AddEntry(hosts[i], true, 1);
    EXPECT_EQ(
        i != 3 ? BlocklistReason::kAllowed
               : BlocklistReason::kUserOptedOutInGeneral,
        block_list_->IsLoadedAndAllowed(hosts[0], 1, false, &passed_reasons_));
    test_clock_.Advance(base::Seconds(1));
  }

  EXPECT_EQ(
      BlocklistReason::kUserOptedOutInGeneral,
      block_list_->IsLoadedAndAllowed(hosts[0], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutInGeneral,
      block_list_->IsLoadedAndAllowed(hosts[1], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutInGeneral,
      block_list_->IsLoadedAndAllowed(hosts[2], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutInGeneral,
      block_list_->IsLoadedAndAllowed(hosts[3], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(hosts[3], 1, true, &passed_reasons_));

  block_list_->AddEntry(hosts[3], false, 1);
  test_clock_.Advance(base::Seconds(1));

  // New non-opt-out entry will cause these to be allowed now.
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(hosts[0], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(hosts[1], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(hosts[2], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(hosts[3], 1, false, &passed_reasons_));
}

TEST_F(OptOutBlocklistTest, QueueBehavior) {
  // Tests the block list asynchronous queue behavior. Methods called while
  // loading the opt-out store are queued and should run in the order they were
  // queued.

  std::vector<bool> test_opt_out{true, false};

  for (auto opt_out : test_opt_out) {
    auto host_policy =
        std::make_unique<BlocklistData::Policy>(base::Days(365), 4u, 2);
    SetHostRule(std::move(host_policy), 5);
    BlocklistData::AllowedTypesAndVersions allowed_types;
    allowed_types.insert({1, 0});
    SetAllowedTypes(std::move(allowed_types));

    StartTest(false /* null_opt_out */);

    EXPECT_EQ(BlocklistReason::kBlocklistNotLoaded,
              block_list_->IsLoadedAndAllowed(kTestHost1, 1, false,
                                              &passed_reasons_));
    block_list_->AddEntry(kTestHost1, opt_out, 1);
    test_clock_.Advance(base::Seconds(1));
    block_list_->AddEntry(kTestHost1, opt_out, 1);
    test_clock_.Advance(base::Seconds(1));
    EXPECT_EQ(BlocklistReason::kBlocklistNotLoaded,
              block_list_->IsLoadedAndAllowed(kTestHost1, 1, false,
                                              &passed_reasons_));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(opt_out ? BlocklistReason::kUserOptedOutOfHost
                      : BlocklistReason::kAllowed,
              block_list_->IsLoadedAndAllowed(kTestHost1, 1, false,
                                              &passed_reasons_));
    block_list_->AddEntry(kTestHost1, opt_out, 1);
    test_clock_.Advance(base::Seconds(1));
    block_list_->AddEntry(kTestHost1, opt_out, 1);
    test_clock_.Advance(base::Seconds(1));
    EXPECT_EQ(0, opt_out_store_->clear_blocklist_count());
    block_list_->ClearBlockList(start_, test_clock_.Now() + base::Seconds(1));
    EXPECT_EQ(1, opt_out_store_->clear_blocklist_count());
    block_list_->AddEntry(kTestHost2, opt_out, 1);
    test_clock_.Advance(base::Seconds(1));
    block_list_->AddEntry(kTestHost2, opt_out, 1);
    test_clock_.Advance(base::Seconds(1));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1, opt_out_store_->clear_blocklist_count());

    EXPECT_EQ(BlocklistReason::kAllowed,
              block_list_->IsLoadedAndAllowed(kTestHost1, 1, false,
                                              &passed_reasons_));
    EXPECT_EQ(opt_out ? BlocklistReason::kUserOptedOutOfHost
                      : BlocklistReason::kAllowed,
              block_list_->IsLoadedAndAllowed(kTestHost2, 1, false,
                                              &passed_reasons_));
  }
}

TEST_F(OptOutBlocklistTest, MaxHosts) {
  // Test that the block list only stores n hosts, and it stores the correct n
  // hosts.
  const std::string test_host_3("host3.com");
  const std::string test_host_4("host4.com");
  const std::string test_host_5("host5.com");

  auto host_policy =
      std::make_unique<BlocklistData::Policy>(base::Days(365), 1u, 1);
  SetHostRule(std::move(host_policy), 2);
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  block_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost2, false, 1);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(test_host_3, false, 1);
  // kTestHost1 should stay in the map, since it has an opt out time.
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfHost,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(test_host_3, 1, false, &passed_reasons_));

  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(test_host_4, true, 1);
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(test_host_5, true, 1);
  // test_host_4 and test_host_5 should remain in the map, but host should be
  // evicted.
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfHost,
      block_list_->IsLoadedAndAllowed(test_host_4, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfHost,
      block_list_->IsLoadedAndAllowed(test_host_5, 1, false, &passed_reasons_));
}

TEST_F(OptOutBlocklistTest, SingleOptOut) {
  // Test that when a user opts out of an action, actions won't be allowed until
  // |single_opt_out_duration| has elapsed.
  int single_opt_out_duration = 5;
  const std::string test_host_3("host3.com");

  auto session_policy = std::make_unique<BlocklistData::Policy>(
      base::Seconds(single_opt_out_duration), 1u, 1);
  SetSessionRule(std::move(session_policy));
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  block_list_->AddEntry(kTestHost1, false, 1);
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(test_host_3, 1, false, &passed_reasons_));

  test_clock_.Advance(base::Seconds(single_opt_out_duration + 1));

  block_list_->AddEntry(kTestHost2, true, 1);
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutInSession,
      block_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutInSession,
      block_list_->IsLoadedAndAllowed(test_host_3, 1, false, &passed_reasons_));

  test_clock_.Advance(base::Seconds(single_opt_out_duration - 1));

  EXPECT_EQ(
      BlocklistReason::kUserOptedOutInSession,
      block_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutInSession,
      block_list_->IsLoadedAndAllowed(test_host_3, 1, false, &passed_reasons_));

  test_clock_.Advance(base::Seconds(single_opt_out_duration + 1));

  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(test_host_3, 1, false, &passed_reasons_));
}

TEST_F(OptOutBlocklistTest, ClearingBlockListClearsRecentNavigation) {
  // Tests that clearing the block list for a long amount of time (relative to
  // "single_opt_out_duration_in_seconds") resets the blocklist's recent opt out
  // rule.

  auto session_policy =
      std::make_unique<BlocklistData::Policy>(base::Seconds(5), 1u, 1);
  SetSessionRule(std::move(session_policy));
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(false /* null_opt_out */);

  block_list_->AddEntry(kTestHost1, true /* opt_out */, 1);
  test_clock_.Advance(base::Seconds(1));
  block_list_->ClearBlockList(start_, test_clock_.Now());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
}

TEST_F(OptOutBlocklistTest, ObserverIsNotifiedOnHostBlocklisted) {
  // Tests the block list behavior when a null OptOutStore is passed in.

  auto host_policy =
      std::make_unique<BlocklistData::Policy>(base::Days(365), 4u, 2);
  SetHostRule(std::move(host_policy), 5);
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));

  // Observer is not notified as blocklisted when the threshold does not met.
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost1, true, 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(blocklist_delegate_.blocklisted_hosts(), ::testing::SizeIs(0));

  // Observer is notified as blocklisted when the threshold is met.
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost1, true, 1);
  base::RunLoop().RunUntilIdle();
  const base::Time blocklisted_time = test_clock_.Now();
  EXPECT_THAT(blocklist_delegate_.blocklisted_hosts(), ::testing::SizeIs(1));
  EXPECT_EQ(blocklisted_time,
            blocklist_delegate_.blocklisted_hosts().find(kTestHost1)->second);

  // Observer is not notified when the host is already blocklisted.
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(kTestHost1, true, 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(blocklist_delegate_.blocklisted_hosts(), ::testing::SizeIs(1));
  EXPECT_EQ(blocklisted_time,
            blocklist_delegate_.blocklisted_hosts().find(kTestHost1)->second);

  // Observer is notified when blocklist is cleared.
  EXPECT_FALSE(blocklist_delegate_.blocklist_cleared());

  test_clock_.Advance(base::Seconds(1));
  block_list_->ClearBlockList(start_, test_clock_.Now());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(blocklist_delegate_.blocklist_cleared());
  EXPECT_EQ(test_clock_.Now(), blocklist_delegate_.blocklist_cleared_time());
}

TEST_F(OptOutBlocklistTest, ObserverIsNotifiedOnUserBlocklisted) {
  // Tests the block list behavior when a null OptOutStore is passed in.
  const std::string hosts[] = {
      "url_0.com",
      "url_1.com",
      "url_2.com",
      "url_3.com",
  };

  int host_indifferent_threshold = 4;

  auto persistent_policy = std::make_unique<BlocklistData::Policy>(
      base::Days(30), 4u, host_indifferent_threshold);
  SetPersistentRule(std::move(persistent_policy));
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  // Initially no host is blocklisted, and user is not blocklisted.
  EXPECT_THAT(blocklist_delegate_.blocklisted_hosts(), ::testing::SizeIs(0));
  EXPECT_FALSE(blocklist_delegate_.user_blocklisted());

  for (int i = 0; i < host_indifferent_threshold; ++i) {
    test_clock_.Advance(base::Seconds(1));
    block_list_->AddEntry(hosts[i], true, 1);
    base::RunLoop().RunUntilIdle();

    EXPECT_THAT(blocklist_delegate_.blocklisted_hosts(), ::testing::SizeIs(0));
    // Observer is notified when number of recently opt out meets
    // |host_indifferent_threshold|.
    EXPECT_EQ(i >= host_indifferent_threshold - 1,
              blocklist_delegate_.user_blocklisted());
  }

  // Observer is notified when the user is no longer blocklisted.
  test_clock_.Advance(base::Seconds(1));
  block_list_->AddEntry(hosts[3], false, 1);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(blocklist_delegate_.user_blocklisted());
}

TEST_F(OptOutBlocklistTest, ObserverIsNotifiedWhenLoadBlocklistDone) {
  int host_indifferent_threshold = 4;
  size_t host_indifferent_history = 4u;
  auto persistent_policy = std::make_unique<BlocklistData::Policy>(
      base::Days(30), host_indifferent_history, host_indifferent_threshold);
  SetPersistentRule(std::move(persistent_policy));
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(false /* null_opt_out */);

  allowed_types.clear();
  allowed_types[0] = 0;
  std::unique_ptr<BlocklistData> data = std::make_unique<BlocklistData>(
      nullptr,
      std::make_unique<BlocklistData::Policy>(base::Seconds(365),
                                              host_indifferent_history,
                                              host_indifferent_threshold),
      nullptr, nullptr, 0, std::move(allowed_types));
  base::SimpleTestClock test_clock;

  for (int i = 0; i < host_indifferent_threshold; ++i) {
    test_clock.Advance(base::Seconds(1));
    data->AddEntry(kTestHost1, true, 0, test_clock.Now(), true);
  }

  std::unique_ptr<TestOptOutStore> opt_out_store =
      std::make_unique<TestOptOutStore>();
  opt_out_store->SetBlocklistData(std::move(data));

  EXPECT_FALSE(blocklist_delegate_.user_blocklisted());
  EXPECT_FALSE(blocklist_delegate_.blocklist_loaded());
  allowed_types.clear();
  allowed_types[1] = 0;
  auto block_list = std::make_unique<TestOptOutBlocklist>(
      std::move(opt_out_store), &test_clock, &blocklist_delegate_);
  block_list->SetAllowedTypes(std::move(allowed_types));

  persistent_policy = std::make_unique<BlocklistData::Policy>(
      base::Days(30), host_indifferent_history, host_indifferent_threshold);
  block_list->SetPersistentRule(std::move(persistent_policy));

  block_list->Init();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(blocklist_delegate_.user_blocklisted());
  EXPECT_TRUE(blocklist_delegate_.blocklist_loaded());
}

TEST_F(OptOutBlocklistTest, ObserverIsNotifiedOfHistoricalBlocklistedHosts) {
  // Tests the block list behavior when a non-null OptOutStore is passed in.
  int host_indifferent_threshold = 2;
  size_t host_indifferent_history = 4u;
  auto host_policy = std::make_unique<BlocklistData::Policy>(
      base::Days(365), host_indifferent_history, host_indifferent_threshold);
  SetHostRule(std::move(host_policy), 5);
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(false /* null_opt_out */);

  base::SimpleTestClock test_clock;

  allowed_types.clear();
  allowed_types[static_cast<int>(1)] = 0;
  std::unique_ptr<BlocklistData> data = std::make_unique<BlocklistData>(
      nullptr, nullptr,
      std::make_unique<BlocklistData::Policy>(base::Days(365),
                                              host_indifferent_history,
                                              host_indifferent_threshold),
      nullptr, 2, std::move(allowed_types));

  test_clock.Advance(base::Seconds(1));
  data->AddEntry(kTestHost1, true, static_cast<int>(1), test_clock.Now(), true);
  test_clock.Advance(base::Seconds(1));
  data->AddEntry(kTestHost1, true, static_cast<int>(1), test_clock.Now(), true);
  base::Time blocklisted_time = test_clock.Now();

  base::RunLoop().RunUntilIdle();
  std::vector<BlocklistReason> reasons;
  EXPECT_NE(BlocklistReason::kAllowed,
            data->IsAllowed(kTestHost1, static_cast<int>(1), false,
                            test_clock.Now(), &reasons));

  // Host |url_b| is not blocklisted.
  test_clock.Advance(base::Seconds(1));
  data->AddEntry(kTestHost2, true, static_cast<int>(1), test_clock.Now(), true);

  std::unique_ptr<TestOptOutStore> opt_out_store =
      std::make_unique<TestOptOutStore>();
  opt_out_store->SetBlocklistData(std::move(data));

  allowed_types.clear();
  allowed_types[static_cast<int>(1)] = 0;
  auto block_list = std::make_unique<TestOptOutBlocklist>(
      std::move(opt_out_store), &test_clock, &blocklist_delegate_);
  block_list->SetAllowedTypes(std::move(allowed_types));

  host_policy = std::make_unique<BlocklistData::Policy>(
      base::Days(30), host_indifferent_history, host_indifferent_threshold);
  block_list->SetPersistentRule(std::move(host_policy));

  block_list->Init();

  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(blocklist_delegate_.blocklisted_hosts(), ::testing::SizeIs(1));
  EXPECT_EQ(blocklisted_time,
            blocklist_delegate_.blocklisted_hosts().find(kTestHost1)->second);
}

TEST_F(OptOutBlocklistTest, PassedReasonsWhenBlocklistDataNotLoaded) {
  // Test that IsLoadedAndAllow, push checked BlocklistReasons to the
  // |passed_reasons| vector.

  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));
  StartTest(false /* null_opt_out */);

  EXPECT_EQ(
      BlocklistReason::kBlocklistNotLoaded,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));

  EXPECT_EQ(0UL, passed_reasons_.size());
}

TEST_F(OptOutBlocklistTest, PassedReasonsWhenUserRecentlyOptedOut) {
  // Test that IsLoadedAndAllow, push checked BlocklistReasons to the
  // |passed_reasons| vector.

  auto session_policy =
      std::make_unique<BlocklistData::Policy>(base::Seconds(5), 1u, 1);
  SetSessionRule(std::move(session_policy));
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  block_list_->AddEntry(kTestHost1, true, 1);
  EXPECT_EQ(
      BlocklistReason::kUserOptedOutInSession,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(1UL, passed_reasons_.size());
  EXPECT_EQ(BlocklistReason::kBlocklistNotLoaded, passed_reasons_[0]);
}

TEST_F(OptOutBlocklistTest, PassedReasonsWhenUserBlocklisted) {
  // Test that IsLoadedAndAllow, push checked BlocklistReasons to the
  // |passed_reasons| vector.
  const std::string hosts[] = {
      "http://www.url_0.com",
      "http://www.url_1.com",
      "http://www.url_2.com",
      "http://www.url_3.com",
  };

  auto session_policy =
      std::make_unique<BlocklistData::Policy>(base::Seconds(1), 1u, 1);
  SetSessionRule(std::move(session_policy));
  auto persistent_policy =
      std::make_unique<BlocklistData::Policy>(base::Days(365), 4u, 4);
  SetPersistentRule(std::move(persistent_policy));
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);
  test_clock_.Advance(base::Seconds(1));

  for (auto host : hosts) {
    block_list_->AddEntry(host, true, 1);
  }

  test_clock_.Advance(base::Seconds(2));

  EXPECT_EQ(
      BlocklistReason::kUserOptedOutInGeneral,
      block_list_->IsLoadedAndAllowed(hosts[0], 1, false, &passed_reasons_));

  BlocklistReason expected_reasons[] = {
      BlocklistReason::kBlocklistNotLoaded,
      BlocklistReason::kUserOptedOutInSession,
  };
  EXPECT_EQ(std::size(expected_reasons), passed_reasons_.size());
  for (size_t i = 0; i < passed_reasons_.size(); i++) {
    EXPECT_EQ(expected_reasons[i], passed_reasons_[i]);
  }
}

TEST_F(OptOutBlocklistTest, PassedReasonsWhenHostBlocklisted) {
  // Test that IsLoadedAndAllow, push checked BlocklistReasons to the
  // |passed_reasons| vector.

  auto session_policy =
      std::make_unique<BlocklistData::Policy>(base::Days(5), 3u, 3);
  SetSessionRule(std::move(session_policy));
  auto persistent_policy =
      std::make_unique<BlocklistData::Policy>(base::Days(365), 4u, 4);
  SetPersistentRule(std::move(persistent_policy));
  auto host_policy =
      std::make_unique<BlocklistData::Policy>(base::Days(30), 4u, 2);
  SetHostRule(std::move(host_policy), 2);
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  block_list_->AddEntry(kTestHost1, true, 1);
  block_list_->AddEntry(kTestHost1, true, 1);

  EXPECT_EQ(
      BlocklistReason::kUserOptedOutOfHost,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));

  BlocklistReason expected_reasons[] = {
      BlocklistReason::kBlocklistNotLoaded,
      BlocklistReason::kUserOptedOutInSession,
      BlocklistReason::kUserOptedOutInGeneral,
  };
  EXPECT_EQ(std::size(expected_reasons), passed_reasons_.size());
  for (size_t i = 0; i < passed_reasons_.size(); i++) {
    EXPECT_EQ(expected_reasons[i], passed_reasons_[i]);
  }
}

TEST_F(OptOutBlocklistTest, PassedReasonsWhenAllowed) {
  // Test that IsLoadedAndAllow, push checked BlocklistReasons to the
  // |passed_reasons| vector.

  auto session_policy =
      std::make_unique<BlocklistData::Policy>(base::Seconds(1), 1u, 1);
  SetSessionRule(std::move(session_policy));
  auto persistent_policy =
      std::make_unique<BlocklistData::Policy>(base::Days(365), 4u, 4);
  SetPersistentRule(std::move(persistent_policy));
  auto host_policy =
      std::make_unique<BlocklistData::Policy>(base::Days(30), 4u, 4);
  SetHostRule(std::move(host_policy), 1);
  auto type_policy =
      std::make_unique<BlocklistData::Policy>(base::Days(30), 4u, 4);
  SetTypeRule(std::move(type_policy));
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  EXPECT_EQ(
      BlocklistReason::kAllowed,
      block_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));

  BlocklistReason expected_reasons[] = {
      BlocklistReason::kBlocklistNotLoaded,
      BlocklistReason::kUserOptedOutInSession,
      BlocklistReason::kUserOptedOutInGeneral,
      BlocklistReason::kUserOptedOutOfHost,
      BlocklistReason::kUserOptedOutOfType,
  };
  EXPECT_EQ(std::size(expected_reasons), passed_reasons_.size());
  for (size_t i = 0; i < passed_reasons_.size(); i++) {
    EXPECT_EQ(expected_reasons[i], passed_reasons_[i]);
  }
}

}  // namespace

}  // namespace blocklist
