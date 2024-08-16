// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/properties.h"

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class DummyNode;

class DummyObserver : public base::CheckedObserver {
 public:
  DummyObserver() = default;
  ~DummyObserver() override = default;

  MOCK_METHOD(void, NotifyAlwaysConst, (const DummyNode*));
  MOCK_METHOD(void, NotifyOnlyOnChangesConst, (const DummyNode*));
  MOCK_METHOD(void,
              NotifyOnlyOnChangesWithPreviousValueConst,
              (const DummyNode*, bool));
};

class DummyNode {
 public:
  DummyNode() = default;
  ~DummyNode() = default;

  void AddObserver(DummyObserver* observer) {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(DummyObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // Fulfills ObservedProperty contract.
  const base::ObserverList<DummyObserver>& GetObservers() { return observers_; }
  bool CanSetProperty() const { return can_set_; }
  bool CanSetAndNotifyProperty() const { return can_set_; }

  void set_can_set(bool can_set) { can_set_ = can_set; }

  bool observed_always() const { return observed_always_.value(); }
  bool observed_only_on_changes() const {
    return observed_only_on_changes_.value();
  }
  bool observed_only_on_changes_with_previous_value() const {
    return observed_only_on_changes_with_previous_value_.value();
  }

  void SetObservedAlwaysNoNotification(bool value) {
    observed_always_.Set(this, value);
  }
  void SetObservedAlways(bool value) {
    observed_always_.SetAndNotify(this, value);
  }
  bool SetObservedOnlyOnChanges(bool value) {
    return observed_only_on_changes_.SetAndMaybeNotify(this, value);
  }
  bool SetObservedOnlyOnChangesWithPreviousValue(bool value) {
    return observed_only_on_changes_with_previous_value_.SetAndMaybeNotify(
        this, value);
  }

 private:
  using ObservedProperty =
      ObservedPropertyImpl<DummyNode, DummyNode, DummyObserver>;

  ObservedProperty::NotifiesAlways<bool, &DummyObserver::NotifyAlwaysConst>
      observed_always_{false};
  ObservedProperty::
      NotifiesOnlyOnChanges<bool, &DummyObserver::NotifyOnlyOnChangesConst>
          observed_only_on_changes_{false};
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      bool,
      bool,
      &DummyObserver::NotifyOnlyOnChangesWithPreviousValueConst>
      observed_only_on_changes_with_previous_value_{false};

  bool can_set_ = true;
  base::ObserverList<DummyObserver> observers_;
};

class GraphPropertiesTest : public ::testing::Test {
 public:
  GraphPropertiesTest() { node_.AddObserver(&observer_); }
  ~GraphPropertiesTest() override { node_.RemoveObserver(&observer_); }

  DummyObserver observer_;
  DummyNode node_;
};

using GraphPropertiesDeathTest = GraphPropertiesTest;

}  // namespace

TEST_F(GraphPropertiesTest, ObservedAlwaysProperty) {
  EXPECT_EQ(false, node_.observed_always());

  EXPECT_CALL(observer_, NotifyAlwaysConst(&node_));
  node_.SetObservedAlways(false);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(false, node_.observed_always());

  EXPECT_CALL(observer_, NotifyAlwaysConst(&node_));
  node_.SetObservedAlways(true);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(true, node_.observed_always());

  EXPECT_CALL(observer_, NotifyAlwaysConst(&node_));
  node_.SetObservedAlways(true);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(true, node_.observed_always());

  testing::Mock::VerifyAndClear(&observer_);
}

TEST_F(GraphPropertiesTest, ObservedOnlyOnChangesProperty) {
  EXPECT_EQ(false, node_.observed_only_on_changes());

  EXPECT_FALSE(node_.SetObservedOnlyOnChanges(false));
  EXPECT_EQ(false, node_.observed_only_on_changes());

  EXPECT_CALL(observer_, NotifyOnlyOnChangesConst(&node_));
  EXPECT_TRUE(node_.SetObservedOnlyOnChanges(true));
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(true, node_.observed_only_on_changes());

  EXPECT_FALSE(node_.SetObservedOnlyOnChanges(true));
  EXPECT_EQ(true, node_.observed_only_on_changes());

  testing::Mock::VerifyAndClear(&observer_);
}

TEST_F(GraphPropertiesTest, ObservedOnlyOnChangesWithPreviousValueProperty) {
  EXPECT_FALSE(node_.observed_only_on_changes_with_previous_value());

  EXPECT_FALSE(node_.SetObservedOnlyOnChangesWithPreviousValue(false));
  EXPECT_EQ(false, node_.observed_only_on_changes_with_previous_value());

  EXPECT_CALL(observer_,
              NotifyOnlyOnChangesWithPreviousValueConst(&node_, false));
  EXPECT_TRUE(node_.SetObservedOnlyOnChangesWithPreviousValue(true));
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(true, node_.observed_only_on_changes_with_previous_value());

  EXPECT_FALSE(node_.SetObservedOnlyOnChangesWithPreviousValue(true));
  EXPECT_EQ(true, node_.observed_only_on_changes_with_previous_value());

  testing::Mock::VerifyAndClear(&observer_);
}

TEST_F(GraphPropertiesDeathTest, DeathOnInvalidSet) {
  node_.set_can_set(false);
  EXPECT_DCHECK_DEATH(node_.SetObservedAlwaysNoNotification(true));
  EXPECT_DCHECK_DEATH(node_.SetObservedAlways(true));
  EXPECT_DCHECK_DEATH(node_.SetObservedOnlyOnChanges(true));
  EXPECT_DCHECK_DEATH(node_.SetObservedOnlyOnChangesWithPreviousValue(true));
}

}  // namespace performance_manager
