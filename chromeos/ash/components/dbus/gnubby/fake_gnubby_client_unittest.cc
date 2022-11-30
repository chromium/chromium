// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/gnubby/fake_gnubby_client.h"

#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/attestation/attestation.pb.h"
#include "chromeos/ash/components/dbus/gnubby/gnubby_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class TestObserver : public GnubbyClient::Observer {
 public:
  TestObserver() = default;

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  void PromptUserAuth() override { calls_++; }
  int calls() { return calls_; }

 private:
  int calls_ = 0;
};

}  // namespace

class FakeGnubbyClientTest : public testing::Test {
 public:
  FakeGnubbyClientTest() = default;

  FakeGnubbyClientTest(const FakeGnubbyClientTest&) = delete;
  FakeGnubbyClientTest& operator=(const FakeGnubbyClientTest&) = delete;

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeGnubbyClient fake_gnubby_client_;
};

TEST_F(FakeGnubbyClientTest, NotificationSent) {
  TestObserver observer;
  base::ScopedObservation<GnubbyClient, GnubbyClient::Observer> scoped_observer(
      &observer);
  scoped_observer.Observe(&fake_gnubby_client_);

  EXPECT_EQ(fake_gnubby_client_.calls(), 0);
  EXPECT_EQ(observer.calls(), 0);
  fake_gnubby_client_.SignalPromptUserAuth();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(fake_gnubby_client_.calls(), 1);
  EXPECT_EQ(observer.calls(), 1);
}

}  // namespace ash
