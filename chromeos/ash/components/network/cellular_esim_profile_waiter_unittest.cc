// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_esim_profile_waiter.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {
const char kTestCarrierProfilePath[] = "/org/chromium/hermes/Profile/1";
}  // namespace

class CellularESimProfileWaiterTest : public testing::Test {
 public:
  CellularESimProfileWaiterTest() = default;
  CellularESimProfileWaiterTest(const CellularESimProfileWaiterTest&) = delete;
  CellularESimProfileWaiterTest& operator=(
      const CellularESimProfileWaiterTest&) = delete;
  ~CellularESimProfileWaiterTest() override = default;

  void SetUp() override {
    hermes_clients::InitializeFakes();
    base::RunLoop().RunUntilIdle();

    esim_profile_waiter_ = std::make_unique<CellularESimProfileWaiter>();
  }

  void TearDown() override { hermes_clients::Shutdown(); }

  void AddCondition(const dbus::ObjectPath& profile_path,
                    CellularESimProfileWaiter::Condition condition) {
    esim_profile_waiter_->AddCondition(profile_path, std::move(condition));
  }

  base::OnceCallback<void()> SetBooleanCallback(bool* value) {
    return base::BindOnce([](bool* value) { *value = true; }, value);
  }

  std::unique_ptr<CellularESimProfileWaiter>& esim_profile_waiter() {
    return esim_profile_waiter_;
  }

  base::test::SingleThreadTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<CellularESimProfileWaiter> esim_profile_waiter_;
};

TEST_F(CellularESimProfileWaiterTest, CalledImmediatelyWhenMissingConditions) {
  bool on_success_called = false;
  bool on_shutdown_called = false;

  esim_profile_waiter()->Wait(
      /*on_success=*/SetBooleanCallback(&on_success_called),
      /*on_shutdown=*/SetBooleanCallback(&on_shutdown_called));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(esim_profile_waiter()->waiting());
  EXPECT_TRUE(on_success_called);
  EXPECT_FALSE(on_shutdown_called);
}

TEST_F(CellularESimProfileWaiterTest, CalledImmediatelyWhenReset) {
  const dbus::ObjectPath kProfilePath(kTestCarrierProfilePath);

  bool on_success_called = false;
  bool on_shutdown_called = false;

  AddCondition(kProfilePath,
               base::BindRepeating(
                   [](HermesProfileClient::Properties*) { return false; }));

  esim_profile_waiter()->Wait(
      /*on_success=*/SetBooleanCallback(&on_success_called),
      /*on_shutdown=*/SetBooleanCallback(&on_shutdown_called));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(esim_profile_waiter()->waiting());
  EXPECT_FALSE(on_success_called);
  EXPECT_FALSE(on_shutdown_called);

  esim_profile_waiter().reset();
  EXPECT_TRUE(on_success_called);
  EXPECT_FALSE(on_shutdown_called);
}

TEST_F(CellularESimProfileWaiterTest, CalledImmediatelyWhenShutdown) {
  const dbus::ObjectPath kProfilePath(kTestCarrierProfilePath);

  bool on_success_called = false;
  bool on_shutdown_called = false;

  AddCondition(kProfilePath,
               base::BindRepeating(
                   [](HermesProfileClient::Properties*) { return false; }));

  esim_profile_waiter()->Wait(
      /*on_success=*/SetBooleanCallback(&on_success_called),
      /*on_shutdown=*/SetBooleanCallback(&on_shutdown_called));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(esim_profile_waiter()->waiting());
  EXPECT_FALSE(on_success_called);
  EXPECT_FALSE(on_shutdown_called);

  static_cast<HermesManagerClient::Observer*>(esim_profile_waiter().get())
      ->OnShutdown();

  EXPECT_FALSE(esim_profile_waiter()->waiting());
  EXPECT_FALSE(on_success_called);
  EXPECT_TRUE(on_shutdown_called);
}

TEST_F(CellularESimProfileWaiterTest, CalledImmediatelyWhenConditionsAreMet) {
  const dbus::ObjectPath kProfilePath(kTestCarrierProfilePath);

  bool on_success_called = false;
  bool on_shutdown_called = false;

  esim_profile_waiter()->RequirePendingProfile(kProfilePath);

  esim_profile_waiter()->Wait(
      /*on_success=*/SetBooleanCallback(&on_success_called),
      /*on_shutdown=*/SetBooleanCallback(&on_shutdown_called));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(esim_profile_waiter()->waiting());
  EXPECT_FALSE(on_success_called);
  EXPECT_FALSE(on_shutdown_called);

  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(kProfilePath);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(esim_profile_waiter()->waiting());
  EXPECT_FALSE(on_success_called);
  EXPECT_FALSE(on_shutdown_called);

  profile_properties->state().ReplaceValue(
      /*value=*/hermes::profile::State::kInactive);
  profile_properties->name().ReplaceValue(
      /*value=*/"");
  profile_properties->activation_code().ReplaceValue(
      /*value=*/"");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(esim_profile_waiter()->waiting());
  EXPECT_FALSE(on_success_called);
  EXPECT_FALSE(on_shutdown_called);

  profile_properties->state().ReplaceValue(
      /*value=*/hermes::profile::State::kPending);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(esim_profile_waiter()->waiting());
  EXPECT_FALSE(on_success_called);
  EXPECT_FALSE(on_shutdown_called);

  profile_properties->name().ReplaceValue(
      /*value=*/"name");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(esim_profile_waiter()->waiting());
  EXPECT_FALSE(on_success_called);
  EXPECT_FALSE(on_shutdown_called);

  profile_properties->activation_code().ReplaceValue(
      /*value=*/"activation_code");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(esim_profile_waiter()->waiting());
  EXPECT_TRUE(on_success_called);
  EXPECT_FALSE(on_shutdown_called);
}

}  // namespace ash
