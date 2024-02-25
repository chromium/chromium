// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/mutable_phone_model.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

namespace {

class FakeObserver : public PhoneModel::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // PhoneModel::Observer:
  void OnModelChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

}  // namespace

class MutablePhoneModelTest : public testing::Test {
 protected:
  MutablePhoneModelTest() = default;
  MutablePhoneModelTest(const MutablePhoneModelTest&) = delete;
  MutablePhoneModelTest& operator=(const MutablePhoneModelTest&) = delete;
  ~MutablePhoneModelTest() override = default;

  // testing::Test:
  void SetUp() override { model_.AddObserver(&fake_observer_); }

  void TearDown() override { model_.RemoveObserver(&fake_observer_); }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 protected:
  MutablePhoneModel model_;

 private:
  FakeObserver fake_observer_;
};

TEST_F(MutablePhoneModelTest, PhoneName) {
  const std::u16string fake_phone_name = u"Phone name";

  // Set the phone name to be null (the default value); observers should
  // not be notified, since this is not a change.
  model_.SetPhoneName(/*phone_name=*/std::nullopt);
  EXPECT_FALSE(model_.phone_name().has_value());
  EXPECT_EQ(0u, GetNumObserverCalls());

  // Set the phone name; observers should be notified.
  model_.SetPhoneName(fake_phone_name);
  EXPECT_EQ(fake_phone_name, model_.phone_name());
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Set the same phone name; observers should not be notified.
  model_.SetPhoneName(fake_phone_name);
  EXPECT_EQ(fake_phone_name, model_.phone_name());
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Set the phone name back to null; observers should be notified.
  model_.SetPhoneName(/*phone_name=*/std::nullopt);
  EXPECT_FALSE(model_.phone_name().has_value());
  EXPECT_EQ(2u, GetNumObserverCalls());
}

TEST_F(MutablePhoneModelTest, PhoneStatusModel) {
  // Set the PhoneStatusModel to be null (the default value); observers should
  // not be notified, since this is not a change.
  model_.SetPhoneStatusModel(/*phone_status_model=*/std::nullopt);
  EXPECT_FALSE(model_.phone_status_model().has_value());
  EXPECT_EQ(0u, GetNumObserverCalls());

  // Set the PhoneStatusModel; observers should be notified.
  model_.SetPhoneStatusModel(CreateFakePhoneStatusModel());
  EXPECT_EQ(CreateFakePhoneStatusModel(), model_.phone_status_model());
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Set the same PhoneStatusModel; observers should not be notified.
  model_.SetPhoneStatusModel(CreateFakePhoneStatusModel());
  EXPECT_EQ(CreateFakePhoneStatusModel(), model_.phone_status_model());
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Set the PhoneStatusModel back to null; observers should be notified.
  model_.SetPhoneStatusModel(/*phone_status_model=*/std::nullopt);
  EXPECT_FALSE(model_.phone_status_model().has_value());
  EXPECT_EQ(2u, GetNumObserverCalls());
}

TEST_F(MutablePhoneModelTest, BrowserTabsModel) {
  // Set the BrowserTabsModel to be null (the default value); observers should
  // not be notified, since this is not a change.
  model_.SetBrowserTabsModel(/*browser_tabs_model=*/std::nullopt);
  EXPECT_FALSE(model_.browser_tabs_model().has_value());
  EXPECT_EQ(0u, GetNumObserverCalls());

  // Set the BrowserTabsModel; observers should be notified.
  model_.SetBrowserTabsModel(CreateFakeBrowserTabsModel());
  EXPECT_EQ(CreateFakeBrowserTabsModel(), model_.browser_tabs_model());
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Set the same BrowserTabsModel; observers should not be notified.
  model_.SetBrowserTabsModel(CreateFakeBrowserTabsModel());
  EXPECT_EQ(CreateFakeBrowserTabsModel(), model_.browser_tabs_model());
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Set the BrowserTabsModel back to null; observers should be notified.
  model_.SetBrowserTabsModel(/*browser_tabs_model=*/std::nullopt);
  EXPECT_FALSE(model_.browser_tabs_model().has_value());
  EXPECT_EQ(2u, GetNumObserverCalls());
}

}  // namespace phonehub
}  // namespace ash
