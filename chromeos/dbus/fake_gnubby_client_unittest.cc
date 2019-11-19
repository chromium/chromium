// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_gnubby_client.h"

#include "base/scoped_observer.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/attestation/attestation.pb.h"
#include "chromeos/dbus/gnubby_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

class TestObserver : public GnubbyClient::Observer {
 public:
  TestObserver() = default;

  void PromptUserAuth() override { calls_++; }
  int calls() { return calls_; }

 private:
  int calls_ = 0;
  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class FakeGnubbyClientTest : public testing::Test {
 public:
  FakeGnubbyClientTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeGnubbyClient fake_gnubby_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeGnubbyClientTest);
};

TEST_F(FakeGnubbyClientTest, NotificationSent) {
  TestObserver observer;
  ScopedObserver<GnubbyClient, GnubbyClient::Observer> scoped_observer(
      &observer);
  scoped_observer.Add(&fake_gnubby_client_);

  EXPECT_EQ(fake_gnubby_client_.calls(), 0);
  EXPECT_EQ(observer.calls(), 0);
  fake_gnubby_client_.SignalPromptUserAuth();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(fake_gnubby_client_.calls(), 1);
  EXPECT_EQ(observer.calls(), 1);
}

}  // namespace chromeos
