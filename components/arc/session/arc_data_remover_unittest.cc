// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_data_remover.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart/fake_upstart_client.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class TestUpstartClient : public chromeos::FakeUpstartClient {
 public:
  TestUpstartClient() = default;
  ~TestUpstartClient() override = default;

  void StartJob(const std::string& job,
                const std::vector<std::string>& upstart_env,
                chromeos::VoidDBusMethodCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), arc_available_));
  }

  void set_arc_available(bool arc_available) { arc_available_ = arc_available; }

 private:
  bool arc_available_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestUpstartClient);
};

class ArcDataRemoverTest : public testing::Test {
 public:
  ArcDataRemoverTest() = default;

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    test_upstart_client_ = std::make_unique<TestUpstartClient>();
    prefs::RegisterProfilePrefs(prefs_.registry());
  }

  void TearDown() override {
    test_upstart_client_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

  PrefService* prefs() { return &prefs_; }

  const cryptohome::Identification& cryptohome_id() const {
    return cryptohome_id_;
  }

  TestUpstartClient* upstart_client() {
    return static_cast<TestUpstartClient*>(chromeos::UpstartClient::Get());
  }

 private:
  TestingPrefServiceSimple prefs_;
  const cryptohome::Identification cryptohome_id_{EmptyAccountId()};
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestUpstartClient> test_upstart_client_;

  DISALLOW_COPY_AND_ASSIGN(ArcDataRemoverTest);
};

TEST_F(ArcDataRemoverTest, NotScheduled) {
  ArcDataRemover data_remover(prefs(), cryptohome_id());

  base::RunLoop loop;
  data_remover.Run(base::BindOnce(
      [](base::RunLoop* loop, base::Optional<bool> result) {
        EXPECT_EQ(result, base::nullopt);
        loop->Quit();
      },
      &loop));
  loop.Run();
}

TEST_F(ArcDataRemoverTest, Success) {
  upstart_client()->set_arc_available(true);

  ArcDataRemover data_remover(prefs(), cryptohome_id());
  data_remover.Schedule();

  base::RunLoop loop;
  data_remover.Run(base::BindOnce(
      [](base::RunLoop* loop, base::Optional<bool> result) {
        EXPECT_EQ(result, base::make_optional(true));
        loop->Quit();
      },
      &loop));
  loop.Run();
}

TEST_F(ArcDataRemoverTest, Fail) {
  ArcDataRemover data_remover(prefs(), cryptohome_id());
  data_remover.Schedule();

  base::RunLoop loop;
  data_remover.Run(base::BindOnce(
      [](base::RunLoop* loop, base::Optional<bool> result) {
        EXPECT_EQ(result, base::make_optional(false));
        loop->Quit();
      },
      &loop));
  loop.Run();
}

TEST_F(ArcDataRemoverTest, PrefPersistsAcrossInstances) {
  {
    ArcDataRemover data_remover(prefs(), cryptohome_id());
    data_remover.Schedule();
    EXPECT_TRUE(data_remover.IsScheduledForTesting());
  }

  {
    ArcDataRemover data_remover(prefs(), cryptohome_id());
    EXPECT_TRUE(data_remover.IsScheduledForTesting());
  }
}

}  // namespace
}  // namespace arc
