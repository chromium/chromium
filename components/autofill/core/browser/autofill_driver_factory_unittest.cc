// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_driver_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class MockAutofillClient : public TestAutofillClient {
 public:
  MOCK_METHOD0(HideAutofillPopup, void());
};

// Just a stub AutofillDriver implementation which announces its construction
// and desctruction by updating the passed |instance_counter|. It also records
// when "user gesture observed" was signalled to it.
class CountingAutofillDriver : public TestAutofillDriver {
 public:
  CountingAutofillDriver(int* instance_counter)
      : instance_counter_(instance_counter) {
    ++*instance_counter;
  }

  ~CountingAutofillDriver() override { --*instance_counter_; }

 private:
  int* const instance_counter_;

  DISALLOW_COPY_AND_ASSIGN(CountingAutofillDriver);
};

// Code-wise this class is identitcal to AutofillDriverFactory, but exposes the
// protected API to the test. Do not modify any of the methods, only include
// "using" declarations to make the AutofillDriverFactory methods public.
class PublicAutofillDriverFactory : public AutofillDriverFactory {
 public:
  explicit PublicAutofillDriverFactory(AutofillClient* client)
      : AutofillDriverFactory(client) {}

  ~PublicAutofillDriverFactory() {}

  using AutofillDriverFactory::AddForKey;
  using AutofillDriverFactory::DeleteForKey;
};

// Wrapper around an integer, checking that the integer is 0 on desctruction.
class CheckedInt {
 public:
  CheckedInt() {}

  ~CheckedInt() { EXPECT_EQ(0, val_); }

  int* val() { return &val_; }

 private:
  int val_ = 0;
};

}  // namespace

class AutofillDriverFactoryTest : public testing::Test {
 public:
  AutofillDriverFactoryTest() : factory_(&client_) {}

  ~AutofillDriverFactoryTest() override {}

  // AutofillDriverFactory stores drivers in a map with keys, which are void*
  // pointers. The factory never dereferences them, so their value does not
  // matter. This is a handy function to create such pointers from integer
  // constants.
  void* KeyFrom(int x) { return reinterpret_cast<void*>(x); }

  // Convenience accessor with a cast to CountingAutofillDriver.
  CountingAutofillDriver* GetDriver(void* key) {
    return static_cast<CountingAutofillDriver*>(factory_.DriverForKey(key));
  }

  std::unique_ptr<AutofillDriver> CreateDriver() {
    ++drivers_created_;
    return std::make_unique<CountingAutofillDriver>(instance_counter_.val());
  }

  base::Callback<std::unique_ptr<AutofillDriver>()> CreateDriverCallback() {
    return base::Bind(&AutofillDriverFactoryTest::CreateDriver,
                      base::Unretained(this));
  }

 protected:
  // For TestAutofillDriver.
  base::test::SingleThreadTaskEnvironment task_environment_;

  MockAutofillClient client_;

  CheckedInt instance_counter_;

  PublicAutofillDriverFactory factory_;

  // How many AutofillDriver instances were created with CreateDriver().
  int drivers_created_ = 0;
};

TEST_F(AutofillDriverFactoryTest, DriverForKey_NoKey) {
  EXPECT_FALSE(factory_.DriverForKey(nullptr));
  EXPECT_FALSE(factory_.DriverForKey(KeyFrom(1)));
}

TEST_F(AutofillDriverFactoryTest, DriverForKey_OneKey) {
  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  EXPECT_FALSE(factory_.DriverForKey(nullptr));
  EXPECT_TRUE(factory_.DriverForKey(KeyFrom(1)));
}

TEST_F(AutofillDriverFactoryTest, DriverForKey_TwoKeys) {
  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  EXPECT_FALSE(factory_.DriverForKey(nullptr));
  EXPECT_TRUE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(1, *instance_counter_.val());

  factory_.AddForKey(nullptr, CreateDriverCallback());
  EXPECT_TRUE(factory_.DriverForKey(nullptr));
  EXPECT_TRUE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(2, *instance_counter_.val());
}

TEST_F(AutofillDriverFactoryTest, AddForKey_Duplicated) {
  EXPECT_FALSE(factory_.DriverForKey(KeyFrom(1)));

  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  EXPECT_TRUE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(1, drivers_created_);
  EXPECT_EQ(1, *instance_counter_.val());

  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  EXPECT_TRUE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(1, drivers_created_);
  EXPECT_EQ(1, *instance_counter_.val());
}

TEST_F(AutofillDriverFactoryTest, DeleteForKey) {
  EXPECT_FALSE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(0, *instance_counter_.val());

  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  EXPECT_TRUE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(1, *instance_counter_.val());

  factory_.DeleteForKey(KeyFrom(1));
  EXPECT_FALSE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(0, *instance_counter_.val());

  // Duplicated calls should raise no errors.
  factory_.DeleteForKey(KeyFrom(1));
  EXPECT_FALSE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(0, *instance_counter_.val());
}

TEST_F(AutofillDriverFactoryTest, NavigationFinished) {
  EXPECT_CALL(client_, HideAutofillPopup());
  factory_.NavigationFinished();
}

TEST_F(AutofillDriverFactoryTest, TabHidden) {
  EXPECT_CALL(client_, HideAutofillPopup());
  factory_.TabHidden();
}

}  // namespace autofill
