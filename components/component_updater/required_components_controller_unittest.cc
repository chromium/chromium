// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/required_components_controller.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/version.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/crx_update_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {
namespace {

class RequiredComponentsControllerTest : public testing::Test {
 public:
  void CreateController(std::vector<std::string> components) {
    controller_ =
        std::make_unique<RequiredComponentsController>(std::move(components));
  }

  ComponentRegistration CreateComponent(std::string_view name,
                                        std::string_view id) {
    return ComponentRegistration(
        std::string(id), std::string(name), /*public_key_hash=*/{},
        base::Version("1.0"),
        /*fingerprint=*/{}, /*installer_attributes=*/{},
        /*action_handler=*/nullptr, /*installer=*/nullptr,
        /*requires_network_encryption=*/false,
        /*supports_group_policy_enable_component_updates=*/true,
        /*allow_cached_copies=*/true,
        /*allow_updates_on_metered_connection=*/true,
        /*allow_updates=*/true);
  }

  bool RequestComponentUpdate(std::string_view name, std::string_view id) {
    ComponentRegistration component = CreateComponent(name, id);
    return controller_->RequestComponentUpdate(component);
  }

  void EnsureRequiredComponentsReady() {
    controller_->EnsureRequiredComponentsReady(base::Seconds(42));
  }

 protected:
  std::unique_ptr<RequiredComponentsController> controller_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(RequiredComponentsControllerTest, ExactNameMatching) {
  CreateController({"Component", "Component2"});

  EXPECT_TRUE(controller_->IsRequiredComponent("Component"));
  EXPECT_TRUE(controller_->IsRequiredComponent("Component2"));
  EXPECT_FALSE(controller_->IsRequiredComponent("FooBar"));
}

TEST_F(RequiredComponentsControllerTest, PartialNameMatching) {
  CreateController({"Comp*", "Component2"});

  EXPECT_TRUE(controller_->IsRequiredComponent("Component"));
  EXPECT_TRUE(controller_->IsRequiredComponent("Component2"));
  EXPECT_FALSE(controller_->IsRequiredComponent("FooBar"));
}

TEST_F(RequiredComponentsControllerTest, AllNameMatching) {
  CreateController({"*", "Component2"});

  EXPECT_TRUE(controller_->IsRequiredComponent("Component"));
  EXPECT_TRUE(controller_->IsRequiredComponent("Component2"));
  EXPECT_TRUE(controller_->IsRequiredComponent("FooBar"));
}

TEST_F(RequiredComponentsControllerTest, ComponentsUpdateDelay) {
  CreateController({"*"});

  ASSERT_TRUE(RequestComponentUpdate("Component", "id"));
  ASSERT_TRUE(RequestComponentUpdate("Component2", "id2"));

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](RequiredComponentsController* controller) {
            update_client::CrxUpdateItem update_item;
            update_item.id = "id";
            update_item.state = update_client::ComponentState::kUpToDate;
            controller->OnEvent(update_item);
          },
          base::Unretained(controller_.get())),
      base::Milliseconds(20));

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](RequiredComponentsController* controller) {
            update_client::CrxUpdateItem update_item;
            update_item.id = "id2";
            update_item.state = update_client::ComponentState::kUpdated;
            controller->OnEvent(update_item);
          },
          base::Unretained(controller_.get())),
      base::Milliseconds(30));

  base::ElapsedTimer timer;
  EnsureRequiredComponentsReady();
  base::TimeDelta elapsed = timer.Elapsed();

  // Component updates simulated above occur at T+20ms and T+30ms, so just
  // verify there is some reasonable delay after T.
  EXPECT_GT(elapsed, base::Milliseconds(10));
}

TEST_F(RequiredComponentsControllerTest, ComponentsUpdateError) {
  CreateController({"*"});

  ASSERT_TRUE(RequestComponentUpdate("Component", "id"));

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](RequiredComponentsController* controller) {
            update_client::CrxUpdateItem update_item;
            update_item.id = "id";
            update_item.state = update_client::ComponentState::kUpdateError;
            update_item.error_code = 42;
            controller->OnEvent(update_item);
          },
          base::Unretained(controller_.get())),
      base::Milliseconds(10));

  EXPECT_DEATH(EnsureRequiredComponentsReady(),
               "Component 'Component' update error, error code: 42");
}

}  // namespace
}  // namespace component_updater
