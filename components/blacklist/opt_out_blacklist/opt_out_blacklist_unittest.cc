// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blacklist/opt_out_blacklist/opt_out_blacklist.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_delegate.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_item.h"
#include "components/blacklist/opt_out_blacklist/opt_out_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blacklist {

namespace {

const char kTestHost1[] = "testhost1.com";
const char kTestHost2[] = "testhost2.com";

// Mock class to test that OptOutBlacklist notifies the delegate with correct
// events (e.g. New host blacklisted, user blacklisted, and blacklist cleared).
class TestOptOutBlacklistDelegate : public OptOutBlacklistDelegate {
 public:
  TestOptOutBlacklistDelegate()
      : user_blacklisted_(false),
        blacklist_cleared_(false),
        blacklist_cleared_time_(base::Time::Now()) {}

  // OptOutBlacklistDelegate:
  void OnNewBlacklistedHost(const std::string& host, base::Time time) override {
    blacklisted_hosts_[host] = time;
  }
  void OnUserBlacklistedStatusChange(bool blacklisted) override {
    user_blacklisted_ = blacklisted;
  }
  void OnBlacklistCleared(base::Time time) override {
    blacklist_cleared_ = true;
    blacklist_cleared_time_ = time;
  }

  // Gets the set of blacklisted hosts recorded.
  const std::unordered_map<std::string, base::Time>& blacklisted_hosts() const {
    return blacklisted_hosts_;
  }

  // Gets the state of user blacklisted status.
  bool user_blacklisted() const { return user_blacklisted_; }

  // Gets the state of blacklisted cleared status of |this| for testing.
  bool blacklist_cleared() const { return blacklist_cleared_; }

  // Gets the event time of blacklist is as cleared.
  base::Time blacklist_cleared_time() const { return blacklist_cleared_time_; }

 private:
  // The user blacklisted status of |this| blacklist_delegate.
  bool user_blacklisted_;

  // Check if the blacklist is notified as cleared on |this| blacklist_delegate.
  bool blacklist_cleared_;

  // The time when blacklist is cleared.
  base::Time blacklist_cleared_time_;

  // |this| blacklist_delegate's collection of blacklisted hosts.
  std::unordered_map<std::string, base::Time> blacklisted_hosts_;
};

class TestOptOutStore : public OptOutStore {
 public:
  TestOptOutStore() : clear_blacklist_count_(0) {}
  ~TestOptOutStore() override {}

  int clear_blacklist_count() { return clear_blacklist_count_; }

  void SetBlacklistData(std::unique_ptr<BlacklistData> data) {
    data_ = std::move(data);
  }

 private:
  // OptOutStore implementation:
  void AddEntry(bool opt_out,
                const std::string& host_name,
                int type,
                base::Time now) override {}

  void LoadBlackList(std::unique_ptr<BlacklistData> blacklist_data,
                     LoadBlackListCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       data_ ? std::move(data_) : std::move(blacklist_data)));
  }

  void ClearBlackList(base::Time begin_time, base::Time end_time) override {
    ++clear_blacklist_count_;
  }

  int clear_blacklist_count_;

  std::unique_ptr<BlacklistData> data_;
};

class TestOptOutBlacklist : public OptOutBlacklist {
 public:
  TestOptOutBlacklist(std::unique_ptr<OptOutStore> opt_out_store,
                      base::Clock* clock,
                      OptOutBlacklistDelegate* blacklist_delegate)
      : OptOutBlacklist(std::move(opt_out_store), clock, blacklist_delegate) {}
  ~TestOptOutBlacklist() override {}

  void SetSessionRule(std::unique_ptr<BlacklistData::Policy> policy) {
    session_policy_ = std::move(policy);
  }

  void SetPersistentRule(std::unique_ptr<BlacklistData::Policy> policy) {
    persistent_policy_ = std::move(policy);
  }

  void SetHostRule(std::unique_ptr<BlacklistData::Policy> policy,
                   size_t max_hosts) {
    host_policy_ = std::move(policy);
    max_hosts_ = max_hosts;
  }

  void SetTypeRule(std::unique_ptr<BlacklistData::Policy> policy) {
    type_policy_ = std::move(policy);
  }

  void SetAllowedTypes(BlacklistData::AllowedTypesAndVersions allowed_types) {
    allowed_types_ = std::move(allowed_types);
  }

 private:
  bool ShouldUseSessionPolicy(base::TimeDelta* duration,
                              size_t* history,
                              int* threshold) const override {
    if (!session_policy_)
      return false;
    *duration = session_policy_->duration;
    *history = session_policy_->history;
    *threshold = session_policy_->threshold;

    return true;
  }

  bool ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                 size_t* history,
                                 int* threshold) const override {
    if (!persistent_policy_)
      return false;
    *duration = persistent_policy_->duration;
    *history = persistent_policy_->history;
    *threshold = persistent_policy_->threshold;

    return true;
  }

  bool ShouldUseHostPolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold,
                           size_t* max_hosts) const override {
    if (!host_policy_)
      return false;
    *duration = host_policy_->duration;
    *history = host_policy_->history;
    *threshold = host_policy_->threshold;
    *max_hosts = max_hosts_;

    return true;
  }

  bool ShouldUseTypePolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold) const override {
    if (!type_policy_)
      return false;
    *duration = type_policy_->duration;
    *history = type_policy_->history;
    *threshold = type_policy_->threshold;

    return true;
  }

  BlacklistData::AllowedTypesAndVersions GetAllowedTypes() const override {
    return allowed_types_;
  }

  std::unique_ptr<BlacklistData::Policy> session_policy_;
  std::unique_ptr<BlacklistData::Policy> persistent_policy_;
  std::unique_ptr<BlacklistData::Policy> host_policy_;
  std::unique_ptr<BlacklistData::Policy> type_policy_;

  size_t max_hosts_ = 0;

  BlacklistData::AllowedTypesAndVersions allowed_types_;
};

class OptOutBlacklistTest : public testing::Test {
 public:
  OptOutBlacklistTest() {}
  ~OptOutBlacklistTest() override {}

  void StartTest(bool null_opt_out_store) {
    std::unique_ptr<TestOptOutStore> opt_out_store =
        null_opt_out_store ? nullptr : std::make_unique<TestOptOutStore>();
    opt_out_store_ = opt_out_store.get();

    black_list_ = std::make_unique<TestOptOutBlacklist>(
        std::move(opt_out_store), &test_clock_, &blacklist_delegate_);
    if (session_policy_) {
      black_list_->SetSessionRule(std::move(session_policy_));
    }
    if (persistent_policy_) {
      black_list_->SetPersistentRule(std::move(persistent_policy_));
    }
    if (host_policy_) {
      black_list_->SetHostRule(std::move(host_policy_), max_hosts_);
    }
    if (type_policy_) {
      black_list_->SetTypeRule(std::move(type_policy_));
    }

    black_list_->SetAllowedTypes(std::move(allowed_types_));
    black_list_->Init();

    start_ = test_clock_.Now();

    passed_reasons_ = {};
  }

  void SetSessionRule(std::unique_ptr<BlacklistData::Policy> policy) {
    session_policy_ = std::move(policy);
  }

  void SetPersistentRule(std::unique_ptr<BlacklistData::Policy> policy) {
    persistent_policy_ = std::move(policy);
  }

  void SetHostRule(std::unique_ptr<BlacklistData::Policy> policy,
                   size_t max_hosts) {
    host_policy_ = std::move(policy);
    max_hosts_ = max_hosts;
  }

  void SetTypeRule(std::unique_ptr<BlacklistData::Policy> policy) {
    type_policy_ = std::move(policy);
  }

  void SetAllowedTypes(BlacklistData::AllowedTypesAndVersions allowed_types) {
    allowed_types_ = std::move(allowed_types);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Observer to |black_list_|.
  TestOptOutBlacklistDelegate blacklist_delegate_;

  base::SimpleTestClock test_clock_;
  TestOptOutStore* opt_out_store_;
  base::Time start_;

  std::unique_ptr<TestOptOutBlacklist> black_list_;
  std::vector<BlacklistReason> passed_reasons_;

 private:
  std::unique_ptr<BlacklistData::Policy> session_policy_;
  std::unique_ptr<BlacklistData::Policy> persistent_policy_;
  std::unique_ptr<BlacklistData::Policy> host_policy_;
  std::unique_ptr<BlacklistData::Policy> type_policy_;

  size_t max_hosts_ = 0;

  BlacklistData::AllowedTypesAndVersions allowed_types_;

  DISALLOW_COPY_AND_ASSIGN(OptOutBlacklistTest);
};

TEST_F(OptOutBlacklistTest, HostBlackListNoStore) {
  // Tests the black list behavior when a null OptOutStore is passed in.
  auto host_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(365), 4u, 2);
  SetHostRule(std::move(host_policy), 5);
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));

  black_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfHost,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, true, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));

  black_list_->AddEntry(kTestHost2, true, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost2, true, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfHost,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, true, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfHost,
      black_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));

  black_list_->AddEntry(kTestHost2, false, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost2, false, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost2, false, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfHost,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, true, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));

  black_list_->ClearBlackList(start_, test_clock_.Now());

  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));
}

TEST_F(OptOutBlacklistTest, TypeBlackListWithStore) {
  // Tests the black list behavior when a non-null OptOutStore is passed in.

  auto type_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(365), 4u, 2);
  SetTypeRule(std::move(type_policy));
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  allowed_types.insert({2, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(false /* null_opt_out */);

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(
      BlacklistReason::kBlacklistNotLoaded,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kBlacklistNotLoaded,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kBlacklistNotLoaded,
      black_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  black_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfType,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfType,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 2, true, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  black_list_->AddEntry(kTestHost1, true, 2);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost1, true, 2);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfType,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfType,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfType,
      black_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  black_list_->AddEntry(kTestHost1, false, 2);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost1, false, 2);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost1, false, 2);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfType,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfType,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  EXPECT_EQ(0, opt_out_store_->clear_blacklist_count());
  black_list_->ClearBlackList(start_, base::Time::Now());
  EXPECT_EQ(1, opt_out_store_->clear_blacklist_count());

  EXPECT_EQ(
      BlacklistReason::kBlacklistNotLoaded,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kBlacklistNotLoaded,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kBlacklistNotLoaded,
      black_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, opt_out_store_->clear_blacklist_count());

  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));
}

TEST_F(OptOutBlacklistTest, TypeBlackListNoStore) {
  // Tests the black list behavior when a null OptOutStore is passed in.
  auto type_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(365), 4u, 2);
  SetTypeRule(std::move(type_policy));
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  allowed_types.insert({2, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  black_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfType,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, true, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  black_list_->AddEntry(kTestHost1, true, 2);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost1, true, 2);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfType,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, true, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfType,
      black_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  black_list_->AddEntry(kTestHost1, false, 2);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost1, false, 2);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost1, false, 2);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfType,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, true, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));

  black_list_->ClearBlackList(start_, test_clock_.Now());

  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 2, false, &passed_reasons_));
}

TEST_F(OptOutBlacklistTest, HostIndifferentBlacklist) {
  // Tests the black list behavior when a null OptOutStore is passed in.
  const std::string hosts[] = {
      "url_0.com", "url_1.com", "url_2.com", "url_3.com",
  };

  int host_indifferent_threshold = 4;

  auto persistent_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(365), 4u, host_indifferent_threshold);
  SetPersistentRule(std::move(persistent_policy));
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(hosts[0], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(hosts[1], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(hosts[2], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(hosts[3], 1, false, &passed_reasons_));

  for (int i = 0; i < host_indifferent_threshold; i++) {
    black_list_->AddEntry(hosts[i], true, 1);
    EXPECT_EQ(
        i != 3 ? BlacklistReason::kAllowed
               : BlacklistReason::kUserOptedOutInGeneral,
        black_list_->IsLoadedAndAllowed(hosts[0], 1, false, &passed_reasons_));
    test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  }

  EXPECT_EQ(
      BlacklistReason::kUserOptedOutInGeneral,
      black_list_->IsLoadedAndAllowed(hosts[0], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutInGeneral,
      black_list_->IsLoadedAndAllowed(hosts[1], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutInGeneral,
      black_list_->IsLoadedAndAllowed(hosts[2], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutInGeneral,
      black_list_->IsLoadedAndAllowed(hosts[3], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(hosts[3], 1, true, &passed_reasons_));

  black_list_->AddEntry(hosts[3], false, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  // New non-opt-out entry will cause these to be allowed now.
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(hosts[0], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(hosts[1], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(hosts[2], 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(hosts[3], 1, false, &passed_reasons_));
}

TEST_F(OptOutBlacklistTest, QueueBehavior) {
  // Tests the black list asynchronous queue behavior. Methods called while
  // loading the opt-out store are queued and should run in the order they were
  // queued.

  std::vector<bool> test_opt_out{true, false};

  for (auto opt_out : test_opt_out) {
    auto host_policy = std::make_unique<BlacklistData::Policy>(
        base::TimeDelta::FromDays(365), 4u, 2);
    SetHostRule(std::move(host_policy), 5);
    BlacklistData::AllowedTypesAndVersions allowed_types;
    allowed_types.insert({1, 0});
    SetAllowedTypes(std::move(allowed_types));

    StartTest(false /* null_opt_out */);

    EXPECT_EQ(BlacklistReason::kBlacklistNotLoaded,
              black_list_->IsLoadedAndAllowed(kTestHost1, 1, false,
                                              &passed_reasons_));
    black_list_->AddEntry(kTestHost1, opt_out, 1);
    test_clock_.Advance(base::TimeDelta::FromSeconds(1));
    black_list_->AddEntry(kTestHost1, opt_out, 1);
    test_clock_.Advance(base::TimeDelta::FromSeconds(1));
    EXPECT_EQ(BlacklistReason::kBlacklistNotLoaded,
              black_list_->IsLoadedAndAllowed(kTestHost1, 1, false,
                                              &passed_reasons_));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(opt_out ? BlacklistReason::kUserOptedOutOfHost
                      : BlacklistReason::kAllowed,
              black_list_->IsLoadedAndAllowed(kTestHost1, 1, false,
                                              &passed_reasons_));
    black_list_->AddEntry(kTestHost1, opt_out, 1);
    test_clock_.Advance(base::TimeDelta::FromSeconds(1));
    black_list_->AddEntry(kTestHost1, opt_out, 1);
    test_clock_.Advance(base::TimeDelta::FromSeconds(1));
    EXPECT_EQ(0, opt_out_store_->clear_blacklist_count());
    black_list_->ClearBlackList(
        start_, test_clock_.Now() + base::TimeDelta::FromSeconds(1));
    EXPECT_EQ(1, opt_out_store_->clear_blacklist_count());
    black_list_->AddEntry(kTestHost2, opt_out, 1);
    test_clock_.Advance(base::TimeDelta::FromSeconds(1));
    black_list_->AddEntry(kTestHost2, opt_out, 1);
    test_clock_.Advance(base::TimeDelta::FromSeconds(1));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1, opt_out_store_->clear_blacklist_count());

    EXPECT_EQ(BlacklistReason::kAllowed,
              black_list_->IsLoadedAndAllowed(kTestHost1, 1, false,
                                              &passed_reasons_));
    EXPECT_EQ(opt_out ? BlacklistReason::kUserOptedOutOfHost
                      : BlacklistReason::kAllowed,
              black_list_->IsLoadedAndAllowed(kTestHost2, 1, false,
                                              &passed_reasons_));
  }
}

TEST_F(OptOutBlacklistTest, MaxHosts) {
  // Test that the black list only stores n hosts, and it stores the correct n
  // hosts.
  const std::string test_host_3("host3.com");
  const std::string test_host_4("host4.com");
  const std::string test_host_5("host5.com");

  auto host_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(365), 1u, 1);
  SetHostRule(std::move(host_policy), 2);
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  black_list_->AddEntry(kTestHost1, true, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost2, false, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(test_host_3, false, 1);
  // kTestHost1 should stay in the map, since it has an opt out time.
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfHost,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(test_host_3, 1, false, &passed_reasons_));

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(test_host_4, true, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(test_host_5, true, 1);
  // test_host_4 and test_host_5 should remain in the map, but host should be
  // evicted.
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfHost,
      black_list_->IsLoadedAndAllowed(test_host_4, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfHost,
      black_list_->IsLoadedAndAllowed(test_host_5, 1, false, &passed_reasons_));
}

TEST_F(OptOutBlacklistTest, SingleOptOut) {
  // Test that when a user opts out of an action, actions won't be allowed until
  // |single_opt_out_duration| has elapsed.
  int single_opt_out_duration = 5;
  const std::string test_host_3("host3.com");

  auto session_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromSeconds(single_opt_out_duration), 1u, 1);
  SetSessionRule(std::move(session_policy));
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  black_list_->AddEntry(kTestHost1, false, 1);
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(test_host_3, 1, false, &passed_reasons_));

  test_clock_.Advance(
      base::TimeDelta::FromSeconds(single_opt_out_duration + 1));

  black_list_->AddEntry(kTestHost2, true, 1);
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutInSession,
      black_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutInSession,
      black_list_->IsLoadedAndAllowed(test_host_3, 1, false, &passed_reasons_));

  test_clock_.Advance(
      base::TimeDelta::FromSeconds(single_opt_out_duration - 1));

  EXPECT_EQ(
      BlacklistReason::kUserOptedOutInSession,
      black_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutInSession,
      black_list_->IsLoadedAndAllowed(test_host_3, 1, false, &passed_reasons_));

  test_clock_.Advance(
      base::TimeDelta::FromSeconds(single_opt_out_duration + 1));

  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost2, 1, false, &passed_reasons_));
  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(test_host_3, 1, false, &passed_reasons_));
}

TEST_F(OptOutBlacklistTest, ClearingBlackListClearsRecentNavigation) {
  // Tests that clearing the black list for a long amount of time (relative to
  // "single_opt_out_duration_in_seconds") resets the blacklist's recent opt out
  // rule.

  auto session_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromSeconds(5), 1u, 1);
  SetSessionRule(std::move(session_policy));
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(false /* null_opt_out */);

  black_list_->AddEntry(kTestHost1, true /* opt_out */, 1);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->ClearBlackList(start_, test_clock_.Now());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
}

TEST_F(OptOutBlacklistTest, ObserverIsNotifiedOnHostBlacklisted) {
  // Tests the black list behavior when a null OptOutStore is passed in.

  auto host_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(365), 4u, 2);
  SetHostRule(std::move(host_policy), 5);
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));

  // Observer is not notified as blacklisted when the threshold does not met.
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost1, true, 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(blacklist_delegate_.blacklisted_hosts(), ::testing::SizeIs(0));

  // Observer is notified as blacklisted when the threshold is met.
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost1, true, 1);
  base::RunLoop().RunUntilIdle();
  const base::Time blacklisted_time = test_clock_.Now();
  EXPECT_THAT(blacklist_delegate_.blacklisted_hosts(), ::testing::SizeIs(1));
  EXPECT_EQ(blacklisted_time,
            blacklist_delegate_.blacklisted_hosts().find(kTestHost1)->second);

  // Observer is not notified when the host is already blacklisted.
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(kTestHost1, true, 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(blacklist_delegate_.blacklisted_hosts(), ::testing::SizeIs(1));
  EXPECT_EQ(blacklisted_time,
            blacklist_delegate_.blacklisted_hosts().find(kTestHost1)->second);

  // Observer is notified when blacklist is cleared.
  EXPECT_FALSE(blacklist_delegate_.blacklist_cleared());

  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->ClearBlackList(start_, test_clock_.Now());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(blacklist_delegate_.blacklist_cleared());
  EXPECT_EQ(test_clock_.Now(), blacklist_delegate_.blacklist_cleared_time());
}

TEST_F(OptOutBlacklistTest, ObserverIsNotifiedOnUserBlacklisted) {
  // Tests the black list behavior when a null OptOutStore is passed in.
  const std::string hosts[] = {
      "url_0.com", "url_1.com", "url_2.com", "url_3.com",
  };

  int host_indifferent_threshold = 4;

  auto persistent_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(30), 4u, host_indifferent_threshold);
  SetPersistentRule(std::move(persistent_policy));
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  // Initially no host is blacklisted, and user is not blacklisted.
  EXPECT_THAT(blacklist_delegate_.blacklisted_hosts(), ::testing::SizeIs(0));
  EXPECT_FALSE(blacklist_delegate_.user_blacklisted());

  for (int i = 0; i < host_indifferent_threshold; ++i) {
    test_clock_.Advance(base::TimeDelta::FromSeconds(1));
    black_list_->AddEntry(hosts[i], true, 1);
    base::RunLoop().RunUntilIdle();

    EXPECT_THAT(blacklist_delegate_.blacklisted_hosts(), ::testing::SizeIs(0));
    // Observer is notified when number of recently opt out meets
    // |host_indifferent_threshold|.
    EXPECT_EQ(i >= host_indifferent_threshold - 1,
              blacklist_delegate_.user_blacklisted());
  }

  // Observer is notified when the user is no longer blacklisted.
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddEntry(hosts[3], false, 1);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(blacklist_delegate_.user_blacklisted());
}

TEST_F(OptOutBlacklistTest, ObserverIsNotifiedWhenLoadBlacklistDone) {
  int host_indifferent_threshold = 4;
  size_t host_indifferent_history = 4u;
  auto persistent_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(30), host_indifferent_history,
      host_indifferent_threshold);
  SetPersistentRule(std::move(persistent_policy));
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(false /* null_opt_out */);

  allowed_types.clear();
  allowed_types[0] = 0;
  std::unique_ptr<BlacklistData> data = std::make_unique<BlacklistData>(
      nullptr,
      std::make_unique<BlacklistData::Policy>(base::TimeDelta::FromSeconds(365),
                                              host_indifferent_history,
                                              host_indifferent_threshold),
      nullptr, nullptr, 0, std::move(allowed_types));
  base::SimpleTestClock test_clock;

  for (int i = 0; i < host_indifferent_threshold; ++i) {
    test_clock.Advance(base::TimeDelta::FromSeconds(1));
    data->AddEntry(kTestHost1, true, 0, test_clock.Now(), true);
  }

  std::unique_ptr<TestOptOutStore> opt_out_store =
      std::make_unique<TestOptOutStore>();
  opt_out_store->SetBlacklistData(std::move(data));

  EXPECT_FALSE(blacklist_delegate_.user_blacklisted());
  allowed_types.clear();
  allowed_types[1] = 0;
  auto black_list = std::make_unique<TestOptOutBlacklist>(
      std::move(opt_out_store), &test_clock, &blacklist_delegate_);
  black_list->SetAllowedTypes(std::move(allowed_types));

  persistent_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(30), host_indifferent_history,
      host_indifferent_threshold);
  black_list->SetPersistentRule(std::move(persistent_policy));

  black_list->Init();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(blacklist_delegate_.user_blacklisted());
}

TEST_F(OptOutBlacklistTest, ObserverIsNotifiedOfHistoricalBlacklistedHosts) {
  // Tests the black list behavior when a non-null OptOutStore is passed in.
  int host_indifferent_threshold = 2;
  size_t host_indifferent_history = 4u;
  auto host_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(365), host_indifferent_history,
      host_indifferent_threshold);
  SetHostRule(std::move(host_policy), 5);
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(false /* null_opt_out */);

  base::SimpleTestClock test_clock;

  allowed_types.clear();
  allowed_types[static_cast<int>(1)] = 0;
  std::unique_ptr<BlacklistData> data = std::make_unique<BlacklistData>(
      nullptr, nullptr,
      std::make_unique<BlacklistData::Policy>(base::TimeDelta::FromDays(365),
                                              host_indifferent_history,
                                              host_indifferent_threshold),
      nullptr, 2, std::move(allowed_types));

  test_clock.Advance(base::TimeDelta::FromSeconds(1));
  data->AddEntry(kTestHost1, true, static_cast<int>(1), test_clock.Now(), true);
  test_clock.Advance(base::TimeDelta::FromSeconds(1));
  data->AddEntry(kTestHost1, true, static_cast<int>(1), test_clock.Now(), true);
  base::Time blacklisted_time = test_clock.Now();

  base::RunLoop().RunUntilIdle();
  std::vector<BlacklistReason> reasons;
  EXPECT_NE(BlacklistReason::kAllowed,
            data->IsAllowed(kTestHost1, static_cast<int>(1), false,
                            test_clock.Now(), &reasons));

  // Host |url_b| is not blacklisted.
  test_clock.Advance(base::TimeDelta::FromSeconds(1));
  data->AddEntry(kTestHost2, true, static_cast<int>(1), test_clock.Now(), true);

  std::unique_ptr<TestOptOutStore> opt_out_store =
      std::make_unique<TestOptOutStore>();
  opt_out_store->SetBlacklistData(std::move(data));

  allowed_types.clear();
  allowed_types[static_cast<int>(1)] = 0;
  auto black_list = std::make_unique<TestOptOutBlacklist>(
      std::move(opt_out_store), &test_clock, &blacklist_delegate_);
  black_list->SetAllowedTypes(std::move(allowed_types));

  host_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(30), host_indifferent_history,
      host_indifferent_threshold);
  black_list->SetPersistentRule(std::move(host_policy));

  black_list->Init();

  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(blacklist_delegate_.blacklisted_hosts(), ::testing::SizeIs(1));
  EXPECT_EQ(blacklisted_time,
            blacklist_delegate_.blacklisted_hosts().find(kTestHost1)->second);
}

TEST_F(OptOutBlacklistTest, PassedReasonsWhenBlacklistDataNotLoaded) {
  // Test that IsLoadedAndAllow, push checked BlacklistReasons to the
  // |passed_reasons| vector.

  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));
  StartTest(false /* null_opt_out */);

  EXPECT_EQ(
      BlacklistReason::kBlacklistNotLoaded,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));

  EXPECT_EQ(0UL, passed_reasons_.size());
}

TEST_F(OptOutBlacklistTest, PassedReasonsWhenUserRecentlyOptedOut) {
  // Test that IsLoadedAndAllow, push checked BlacklistReasons to the
  // |passed_reasons| vector.

  auto session_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromSeconds(5), 1u, 1);
  SetSessionRule(std::move(session_policy));
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  black_list_->AddEntry(kTestHost1, true, 1);
  EXPECT_EQ(
      BlacklistReason::kUserOptedOutInSession,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));
  EXPECT_EQ(1UL, passed_reasons_.size());
  EXPECT_EQ(BlacklistReason::kBlacklistNotLoaded, passed_reasons_[0]);
}

TEST_F(OptOutBlacklistTest, PassedReasonsWhenUserBlacklisted) {
  // Test that IsLoadedAndAllow, push checked BlacklistReasons to the
  // |passed_reasons| vector.
  const std::string hosts[] = {
      "http://www.url_0.com", "http://www.url_1.com", "http://www.url_2.com",
      "http://www.url_3.com",
  };

  auto session_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromSeconds(1), 1u, 1);
  SetSessionRule(std::move(session_policy));
  auto persistent_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(365), 4u, 4);
  SetPersistentRule(std::move(persistent_policy));
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));

  for (auto host : hosts) {
    black_list_->AddEntry(host, true, 1);
  }

  test_clock_.Advance(base::TimeDelta::FromSeconds(2));

  EXPECT_EQ(
      BlacklistReason::kUserOptedOutInGeneral,
      black_list_->IsLoadedAndAllowed(hosts[0], 1, false, &passed_reasons_));

  BlacklistReason expected_reasons[] = {
      BlacklistReason::kBlacklistNotLoaded,
      BlacklistReason::kUserOptedOutInSession,
  };
  EXPECT_EQ(base::size(expected_reasons), passed_reasons_.size());
  for (size_t i = 0; i < passed_reasons_.size(); i++) {
    EXPECT_EQ(expected_reasons[i], passed_reasons_[i]);
  }
}

TEST_F(OptOutBlacklistTest, PassedReasonsWhenHostBlacklisted) {
  // Test that IsLoadedAndAllow, push checked BlacklistReasons to the
  // |passed_reasons| vector.

  auto session_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(5), 3u, 3);
  SetSessionRule(std::move(session_policy));
  auto persistent_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(365), 4u, 4);
  SetPersistentRule(std::move(persistent_policy));
  auto host_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(30), 4u, 2);
  SetHostRule(std::move(host_policy), 2);
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  black_list_->AddEntry(kTestHost1, true, 1);
  black_list_->AddEntry(kTestHost1, true, 1);

  EXPECT_EQ(
      BlacklistReason::kUserOptedOutOfHost,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));

  BlacklistReason expected_reasons[] = {
      BlacklistReason::kBlacklistNotLoaded,
      BlacklistReason::kUserOptedOutInSession,
      BlacklistReason::kUserOptedOutInGeneral,
  };
  EXPECT_EQ(base::size(expected_reasons), passed_reasons_.size());
  for (size_t i = 0; i < passed_reasons_.size(); i++) {
    EXPECT_EQ(expected_reasons[i], passed_reasons_[i]);
  }
}

TEST_F(OptOutBlacklistTest, PassedReasonsWhenAllowed) {
  // Test that IsLoadedAndAllow, push checked BlacklistReasons to the
  // |passed_reasons| vector.

  auto session_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromSeconds(1), 1u, 1);
  SetSessionRule(std::move(session_policy));
  auto persistent_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(365), 4u, 4);
  SetPersistentRule(std::move(persistent_policy));
  auto host_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(30), 4u, 4);
  SetHostRule(std::move(host_policy), 1);
  auto type_policy = std::make_unique<BlacklistData::Policy>(
      base::TimeDelta::FromDays(30), 4u, 4);
  SetTypeRule(std::move(type_policy));
  BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetAllowedTypes(std::move(allowed_types));

  StartTest(true /* null_opt_out */);

  EXPECT_EQ(
      BlacklistReason::kAllowed,
      black_list_->IsLoadedAndAllowed(kTestHost1, 1, false, &passed_reasons_));

  BlacklistReason expected_reasons[] = {
      BlacklistReason::kBlacklistNotLoaded,
      BlacklistReason::kUserOptedOutInSession,
      BlacklistReason::kUserOptedOutInGeneral,
      BlacklistReason::kUserOptedOutOfHost,
      BlacklistReason::kUserOptedOutOfType,
  };
  EXPECT_EQ(base::size(expected_reasons), passed_reasons_.size());
  for (size_t i = 0; i < passed_reasons_.size(); i++) {
    EXPECT_EQ(expected_reasons[i], passed_reasons_[i]);
  }
}

}  // namespace

}  // namespace blacklist
