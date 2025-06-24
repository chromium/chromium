// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/ios/content_rule_list_data.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace script_blocking {

namespace {

// A mock observer to verify notification logic.
class MockObserver : public ContentRuleListData::Observer {
 public:
  MOCK_METHOD(void,
              OnScriptBlockingRuleListUpdated,
              (const std::string& rules_json),
              (override));
};

class ScopedObserver {
 public:
  ScopedObserver(ContentRuleListData& source,
                 ContentRuleListData::Observer& observer)
      : observation_(&observer) {
    observation_.Observe(&source);
  }

 private:
  base::ScopedObservation<ContentRuleListData, ContentRuleListData::Observer>
      observation_;
};

}  // namespace

class ContentRuleListDataTest : public testing::Test {
 public:
  ContentRuleListDataTest() : data_(ContentRuleListData::GetInstance()) {}

 protected:
  void SetUp() override { data_.get().ResetForTesting(); }

  base::test::TaskEnvironment task_environment_;
  raw_ref<ContentRuleListData> data_;
};

// Tests that initially, no content rule list is available.
TEST_F(ContentRuleListDataTest, GetContentRuleList_InitialReturnsNull) {
  EXPECT_EQ(data_->GetContentRuleList(), std::nullopt);
}

// Tests that setting the rule list makes it available via the getter.
TEST_F(ContentRuleListDataTest, SetAndGetContentRuleList) {
  const std::string test_rules = R"([{"id": 1}])";

  data_->SetContentRuleList(test_rules);
  std::optional<std::string> retrieved_rules = data_->GetContentRuleList();

  ASSERT_TRUE(retrieved_rules.has_value());
  EXPECT_EQ(*retrieved_rules, test_rules);
}

// Tests that an observer is notified when the rule list is updated.
TEST_F(ContentRuleListDataTest, AddObserver_NotifiesOnSet) {
  testing::StrictMock<MockObserver> observer;
  ScopedObserver scoped_observer(data_.get(), observer);

  const std::string test_rules = R"([{"id": 1, "action": "block"}])";
  EXPECT_CALL(observer, OnScriptBlockingRuleListUpdated(test_rules));

  data_->SetContentRuleList(test_rules);
}

// Tests that a new observer is notified immediately if data already exists.
TEST_F(ContentRuleListDataTest, AddObserver_NotifiesImmediatelyIfDataExists) {
  const std::string test_rules = R"([{"id": 2}])";
  data_->SetContentRuleList(test_rules);

  testing::StrictMock<MockObserver> observer;
  // The observer should be notified immediately, because it adds itself as an
  // observer during its construction, and data is already available.
  EXPECT_CALL(observer, OnScriptBlockingRuleListUpdated(test_rules));
  ScopedObserver scoped_observer(data_.get(), observer);
}

// Tests that removing an observer stops it from receiving notifications.
TEST_F(ContentRuleListDataTest, RemoveObserver_StopsNotifications) {
  testing::StrictMock<MockObserver> observer;
  {
    // The ScopedObserver is created and then destroyed when it goes out of
    // this scope, which automatically removes the observer via its internal
    // base::ScopedObservation member.
    ScopedObserver scoped_observer(data_.get(), observer);
  }

  // StrictMock will fail the test if a notification occurs.
  data_->SetContentRuleList(R"([{"id": 3}])");
}

// Tests that multiple observers are all notified of an update.
TEST_F(ContentRuleListDataTest, SetContentRuleList_NotifiesMultipleObservers) {
  testing::StrictMock<MockObserver> observer1;
  testing::StrictMock<MockObserver> observer2;
  ScopedObserver scoped_observer1(data_.get(), observer1);
  ScopedObserver scoped_observer2(data_.get(), observer2);

  const std::string test_rules = R"([{"id": 4}])";
  EXPECT_CALL(observer1, OnScriptBlockingRuleListUpdated(test_rules));
  EXPECT_CALL(observer2, OnScriptBlockingRuleListUpdated(test_rules));

  data_->SetContentRuleList(test_rules);
}

}  // namespace script_blocking
