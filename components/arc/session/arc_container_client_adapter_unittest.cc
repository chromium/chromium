// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_helpers.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/arc/session/arc_container_client_adapter.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class ArcContainerClientAdapterTest : public testing::Test {
 public:
  ArcContainerClientAdapterTest() = default;
  ~ArcContainerClientAdapterTest() override = default;
  ArcContainerClientAdapterTest(const ArcContainerClientAdapterTest&) = delete;
  ArcContainerClientAdapterTest& operator=(
      const ArcContainerClientAdapterTest&) = delete;

  void SetUp() override {
    chromeos::SessionManagerClient::InitializeFake();
    client_adapter_ = CreateArcContainerClientAdapter();
    chromeos::FakeSessionManagerClient::Get()->set_arc_available(true);
  }

  void TearDown() override {
    client_adapter_ = nullptr;
    chromeos::SessionManagerClient::Shutdown();
  }

 protected:
  ArcClientAdapter* client_adapter() { return client_adapter_.get(); }

 private:
  std::unique_ptr<ArcClientAdapter> client_adapter_;
  content::BrowserTaskEnvironment browser_task_environment_;
};

void OnMiniInstanceStarted(bool result) {
  DCHECK(result);
}

// b/164816080 This test ensures that a new container instance that is
// created while handling the shutting down of the previous instance,
// doesn't incorrectly receive the shutdown event as well.
TEST_F(ArcContainerClientAdapterTest,
       DoesNotGetArcInstanceStoppedOnNestedInstance) {
  class Observer : public ArcClientAdapter::Observer {
   public:
    explicit Observer(Observer* child_observer)
        : child_observer_(child_observer) {}
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    ~Observer() override {
      if (child_observer_ && nested_client_adapter_)
        nested_client_adapter_->RemoveObserver(child_observer_);
    }

    bool stopped_called() const { return stopped_called_; }

    // ArcClientAdapter::Observer:
    void ArcInstanceStopped() override {
      stopped_called_ = true;

      if (child_observer_) {
        nested_client_adapter_ = CreateArcContainerClientAdapter();
        nested_client_adapter_->AddObserver(child_observer_);
      }
    }

   private:
    Observer* const child_observer_;
    std::unique_ptr<ArcClientAdapter> nested_client_adapter_;
    bool stopped_called_ = false;
  };

  Observer child_observer(nullptr);
  Observer parent_observer(&child_observer);
  client_adapter()->AddObserver(&parent_observer);
  base::ScopedClosureRunner teardown(base::BindOnce(
      [](ArcClientAdapter* client_adapter, Observer* parent_observer) {
        client_adapter->RemoveObserver(parent_observer);
      },
      client_adapter(), &parent_observer));

  chromeos::FakeSessionManagerClient::Get()->NotifyArcInstanceStopped();

  EXPECT_TRUE(parent_observer.stopped_called());
  EXPECT_FALSE(child_observer.stopped_called());
}

TEST_F(ArcContainerClientAdapterTest, StartArc_DisableMediaStoreMaintenance) {
  StartParams start_params;
  start_params.disable_media_store_maintenance = true;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = chromeos::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_TRUE(request.has_disable_media_store_maintenance());
  EXPECT_TRUE(request.disable_media_store_maintenance());
}

struct DalvikMemoryProfileTestParam {
  // Requested profile.
  StartParams::DalvikMemoryProfile profile;
  // Expected value passed to DBus.
  login_manager::StartArcMiniContainerRequest_DalvikMemoryProfile expectation;
};

constexpr DalvikMemoryProfileTestParam kDalvikMemoryProfileTestCases[] = {
    {StartParams::DalvikMemoryProfile::DEFAULT,
     login_manager::
         StartArcMiniContainerRequest_DalvikMemoryProfile_MEMORY_PROFILE_DEFAULT},
    {StartParams::DalvikMemoryProfile::M4G,
     login_manager::
         StartArcMiniContainerRequest_DalvikMemoryProfile_MEMORY_PROFILE_4G},
    {StartParams::DalvikMemoryProfile::M8G,
     login_manager::
         StartArcMiniContainerRequest_DalvikMemoryProfile_MEMORY_PROFILE_8G},
    {StartParams::DalvikMemoryProfile::M16G,
     login_manager::
         StartArcMiniContainerRequest_DalvikMemoryProfile_MEMORY_PROFILE_16G}};

class ArcContainerClientAdapterDalvikMemoryProfileTest
    : public ArcContainerClientAdapterTest,
      public testing::WithParamInterface<DalvikMemoryProfileTestParam> {};

INSTANTIATE_TEST_SUITE_P(All,
                         ArcContainerClientAdapterDalvikMemoryProfileTest,
                         ::testing::ValuesIn(kDalvikMemoryProfileTestCases));


TEST_P(ArcContainerClientAdapterDalvikMemoryProfileTest, Profile) {
  const auto& test_param = GetParam();
  StartParams start_params;
  start_params.dalvik_memory_profile = test_param.profile;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = chromeos::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_TRUE(request.has_dalvik_memory_profile());
  EXPECT_EQ(test_param.expectation, request.dalvik_memory_profile());
}

}  // namespace

}  // namespace arc
