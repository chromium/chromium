// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/instance.h"

#include <utility>

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

constexpr char kAppId[] = "abcdefgh";
constexpr char kLaunchId[] = "abc";

}  // namespace

// Unit tests for restore data.
class InstanceTest : public testing::Test {
 public:
  InstanceTest() = default;
  InstanceTest(const InstanceTest&) = delete;
  InstanceTest& operator=(const InstanceTest&) = delete;
  ~InstanceTest() override = default;

  void SetInstanceId(Instance* instance,
                     const base::UnguessableToken& instance_id) {
    instance->instance_id_ = instance_id;
  }

  void VerifyInstance(Instance* instance,
                      const std::string& app_id,
                      const base::UnguessableToken& instance_id,
                      aura::Window* window) {
    ASSERT_TRUE(instance);
    EXPECT_EQ(app_id, instance->AppId());
    EXPECT_EQ(instance_id, instance->InstanceId());
    EXPECT_EQ(window, instance->Window());
    EXPECT_TRUE(instance->LaunchId().empty());
    EXPECT_EQ(InstanceState::kUnknown, instance->State());
    EXPECT_FALSE(instance->BrowserContext());
  }

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(InstanceTest, CreateInstanceWithInstanceId) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  base::UnguessableToken instance_id = base::UnguessableToken::Create();

  auto instance1 = std::make_unique<Instance>(kAppId, instance_id, &window);
  std::unique_ptr<Instance> instance2 = instance1->Clone();

  VerifyInstance(instance2.get(), kAppId, instance_id, &window);
}

TEST_F(InstanceTest, ModifyWindow) {
  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  base::UnguessableToken instance_id = base::UnguessableToken::Create();

  auto instance1 = std::make_unique<Instance>(kAppId, instance_id, &window1);

  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  instance1->SetWindow(&window2);
  std::unique_ptr<Instance> instance2 = instance1->Clone();

  VerifyInstance(instance2.get(), kAppId, instance_id, &window2);
}

TEST_F(InstanceTest, AllFields) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  base::UnguessableToken instance_id = base::UnguessableToken::Create();

  auto instance1 = std::make_unique<Instance>(kAppId, instance_id, &window);

  SetInstanceId(instance1.get(), instance_id);

  instance1->SetLaunchId(kLaunchId);
  base::Time current_time = base::Time::Now();
  instance1->UpdateState(InstanceState::kActive, current_time);
  TestingProfile profile;
  instance1->SetBrowserContext(&profile);

  std::unique_ptr<Instance> instance2 = instance1->Clone();

  EXPECT_EQ(kAppId, instance2->AppId());
  EXPECT_EQ(instance_id, instance2->InstanceId());
  EXPECT_EQ(&window, instance2->Window());
  EXPECT_EQ(kLaunchId, instance2->LaunchId());
  EXPECT_EQ(InstanceState::kActive, instance2->State());
  EXPECT_EQ(current_time, instance2->LastUpdatedTime());
  EXPECT_EQ(&profile, instance2->BrowserContext());
}

}  // namespace apps
