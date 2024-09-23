// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"

class InstanceRegistryTest : public testing::Test,
                             public apps::InstanceRegistry::Observer {
 protected:
  void MakeInstanceWithWindow(
      const char* app_id,
      aura::Window* window,
      apps::InstanceState state = apps::InstanceState::kUnknown,
      base::Time time = base::Time()) {
    MakeInstance(app_id, window, state, time);
  }

  void MakeInstance(const char* app_id,
                    aura::Window* window,
                    apps::InstanceState state = apps::InstanceState::kUnknown,
                    base::Time time = base::Time()) {
    apps::InstanceParams params(app_id, window);
    params.state = std::make_pair(state, time);
    instance_registry_.CreateOrUpdateInstance(std::move(params));
  }

  void CallForEachInstance(apps::InstanceRegistry& instance_registry) {
    instance_registry.ForEachInstance(
        [this](const apps::InstanceUpdate& update) {
          OnInstanceUpdate(update);
        });
  }

  std::set<apps::InstanceState> GetStates(const std::string& app_id,
                                          const aura::Window* window) {
    std::set<apps::InstanceState> states;
    instance_registry_.ForInstancesWithWindow(
        window, [&](const apps::InstanceUpdate& update) {
          if (update.AppId() == app_id) {
            states.insert(update.State());
          }
        });
    return states;
  }

  // apps::InstanceRegistry::Observer overrides.
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override {
    EXPECT_NE("", update.AppId());
    if (update.StateChanged() &&
        update.State() == apps::InstanceState::kRunning) {
      num_running_apps_++;
    }
    updated_ids_.insert(update.AppId());
    updated_enclosing_windows_.insert(update.Window());
  }

  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* instance_registry) override {
    // The test code explicitly calls both AddObserver and RemoveObserver.
    NOTREACHED_IN_MIGRATION();
  }

  apps::InstanceRegistry& instance_registry() { return instance_registry_; }

  int num_running_apps_ = 0;
  std::set<std::string> updated_ids_;
  std::set<raw_ptr<const aura::Window, SetExperimental>>
      updated_enclosing_windows_;

  apps::InstanceRegistry instance_registry_;
};

// In the tests below, just "recursive" means that instance_registry.OnInstance
// calls observer.OnInstanceUpdate which calls instance_registry.ForEachInstance
// and instance_registry.ForOneInstance. "Super-recursive" means that
// instance_registry.OnInstance calls observer.OnInstanceUpdate calls
// instance_registry.OnInstance which calls observer.OnInstanceUpdate.
class InstanceRecursiveObserver : public apps::InstanceRegistry::Observer {
 public:
  explicit InstanceRecursiveObserver(apps::InstanceRegistry* instance_registry)
      : instance_registry_(instance_registry) {
    instance_registry_observation_.Observe(instance_registry);
  }

  ~InstanceRecursiveObserver() override = default;

  void PrepareParamsForOnInstances(
      int expected_num_instances,
      std::vector<std::unique_ptr<apps::InstanceParams>>*
          super_recursive_instance_params = nullptr) {
    expected_num_instances_ = expected_num_instances;
    num_instances_seen_on_instance_update_ = 0;

    if (super_recursive_instance_params) {
      super_recursive_instance_params_.swap(*super_recursive_instance_params);
    }
  }

  void PrepareForOnInstances(int expected_num_instances,
                             std::vector<std::unique_ptr<apps::Instance>>*
                                 super_recursive_instances = nullptr) {
    expected_num_instances_ = expected_num_instances;
    num_instances_seen_on_instance_update_ = 0;

    if (super_recursive_instances) {
      super_recursive_instances_.swap(*super_recursive_instances);
    }
  }

  int NumInstancesSeenOnInstanceUpdate() {
    return num_instances_seen_on_instance_update_;
  }

 protected:
  // apps::InstanceRegistry::Observer overrides.
  void OnInstanceUpdate(const apps::InstanceUpdate& outer) override {
    int num_instance = 0;
    instance_registry_->ForEachInstance(
        [&outer, &num_instance](const apps::InstanceUpdate& inner) {
          if (outer.InstanceId() == inner.InstanceId()) {
            ExpectEq(outer, inner);
          }
          num_instance++;
        });

    EXPECT_TRUE(instance_registry_->ForOneInstance(
        outer.InstanceId(), [&outer](const apps::InstanceUpdate& inner) {
          ExpectEq(outer, inner);
        }));

    EXPECT_TRUE(instance_registry_->ForInstancesWithWindow(
        outer.Window(), [&outer](const apps::InstanceUpdate& inner) {
          if (outer.InstanceId() == inner.InstanceId()) {
            ExpectEq(outer, inner);
          }
        }));

    if (expected_num_instances_ >= 0) {
      EXPECT_EQ(expected_num_instances_, num_instance);
    }

    while (!super_recursive_instance_params_.empty()) {
      std::unique_ptr<apps::InstanceParams> params =
          std::move(super_recursive_instance_params_.back());
      if (params.get() == nullptr) {
        // This is the placeholder 'punctuation'.
        super_recursive_instance_params_.pop_back();
        break;
      }
      instance_registry_->CreateOrUpdateInstance(std::move(*params));
      super_recursive_instance_params_.pop_back();
    }

    while (!super_recursive_instances_.empty()) {
      std::unique_ptr<apps::Instance> instance =
          std::move(super_recursive_instances_.back());
      if (instance.get() == nullptr) {
        // This is the placeholder 'punctuation'.
        super_recursive_instances_.pop_back();
        break;
      }
      instance_registry_->OnInstance(std::move(instance));
      super_recursive_instances_.pop_back();
    }

    num_instances_seen_on_instance_update_++;
  }

  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* instance_registry) override {
    instance_registry_observation_.Reset();
  }

  static void ExpectEq(const apps::InstanceUpdate& outer,
                       const apps::InstanceUpdate& inner) {
    EXPECT_EQ(outer.AppId(), inner.AppId());
    EXPECT_EQ(outer.InstanceId(), inner.InstanceId());
    EXPECT_EQ(outer.Window(), inner.Window());
    EXPECT_EQ(outer.LaunchId(), inner.LaunchId());
    EXPECT_EQ(outer.State(), inner.State());
    EXPECT_EQ(outer.LastUpdatedTime(), inner.LastUpdatedTime());
    EXPECT_EQ(outer.BrowserContext(), inner.BrowserContext());
  }

  raw_ptr<apps::InstanceRegistry> instance_registry_;

  int expected_num_instances_ = -1;
  int num_instances_seen_on_instance_update_ = 0;

  // Non-empty when this.OnInstanceUpdate should trigger more
  // instance_registry_.OnInstance calls.
  //
  // During OnInstanceUpdate, this vector (a stack) is popped from the back
  // until a nullptr 'punctuation' element (a group terminator) is seen. If that
  // group of popped elements (in LIFO order) is non-empty, that group forms the
  // vector of App's passed to instance_registry_.OnInstance.
  std::vector<std::unique_ptr<apps::InstanceParams>>
      super_recursive_instance_params_;
  std::vector<std::unique_ptr<apps::Instance>> super_recursive_instances_;

  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      instance_registry_observation_{this};
};

TEST_F(InstanceRegistryTest, ForEachInstance) {
  updated_enclosing_windows_.clear();
  updated_ids_.clear();

  CallForEachInstance(instance_registry());

  EXPECT_EQ(0u, updated_enclosing_windows_.size());
  EXPECT_EQ(0u, updated_ids_.size());

  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window3(nullptr);
  window3.Init(ui::LAYER_NOT_DRAWN);
  MakeInstanceWithWindow("a", &window1);
  MakeInstanceWithWindow("b", &window2);
  MakeInstanceWithWindow("c", &window3);

  updated_enclosing_windows_.clear();
  updated_ids_.clear();
  CallForEachInstance(instance_registry());

  EXPECT_EQ(3u, updated_enclosing_windows_.size());
  EXPECT_EQ(3u, updated_ids_.size());
  EXPECT_NE(updated_enclosing_windows_.end(),
            updated_enclosing_windows_.find(&window1));
  EXPECT_NE(updated_enclosing_windows_.end(),
            updated_enclosing_windows_.find(&window2));
  EXPECT_NE(updated_enclosing_windows_.end(),
            updated_enclosing_windows_.find(&window3));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("a"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("b"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));

  aura::Window window4(nullptr);
  window4.Init(ui::LAYER_NOT_DRAWN);
  MakeInstanceWithWindow("a", &window1, apps::InstanceState::kRunning);
  MakeInstanceWithWindow("c", &window4);

  updated_enclosing_windows_.clear();
  updated_ids_.clear();
  CallForEachInstance(instance_registry());

  EXPECT_EQ(4u, updated_enclosing_windows_.size());
  EXPECT_EQ(3u, updated_ids_.size());
  EXPECT_NE(updated_enclosing_windows_.end(),
            updated_enclosing_windows_.find(&window1));
  EXPECT_NE(updated_enclosing_windows_.end(),
            updated_enclosing_windows_.find(&window2));
  EXPECT_NE(updated_enclosing_windows_.end(),
            updated_enclosing_windows_.find(&window3));
  EXPECT_NE(updated_enclosing_windows_.end(),
            updated_enclosing_windows_.find(&window4));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("a"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("b"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));

  // Test that ForOneApp succeeds for window4 and fails for window5.

  bool found_window4 = false;
  EXPECT_TRUE(instance_registry().ForInstancesWithWindow(
      &window4, [&found_window4](const apps::InstanceUpdate& update) {
        found_window4 = true;
        EXPECT_EQ("c", update.AppId());
      }));
  EXPECT_TRUE(found_window4);

  bool found_window5 = false;
  aura::Window window5(nullptr);
  window5.Init(ui::LAYER_NOT_DRAWN);
  EXPECT_FALSE(instance_registry().ForInstancesWithWindow(
      &window5, [&found_window5](const apps::InstanceUpdate& update) {
        found_window5 = true;
      }));
  EXPECT_FALSE(found_window5);
}

TEST_F(InstanceRegistryTest, Observer) {
  instance_registry().AddObserver(this);

  num_running_apps_ = 0;
  updated_enclosing_windows_.clear();
  updated_ids_.clear();

  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window3(nullptr);
  window3.Init(ui::LAYER_NOT_DRAWN);

  MakeInstanceWithWindow("a", &window1);
  MakeInstanceWithWindow("c", &window2);
  MakeInstanceWithWindow("a", &window3);

  EXPECT_EQ(0, num_running_apps_);
  EXPECT_EQ(3u, updated_enclosing_windows_.size());
  EXPECT_EQ(2u, updated_ids_.size());
  EXPECT_NE(updated_enclosing_windows_.end(),
            updated_enclosing_windows_.find(&window1));
  EXPECT_NE(updated_enclosing_windows_.end(),
            updated_enclosing_windows_.find(&window2));
  EXPECT_NE(updated_enclosing_windows_.end(),
            updated_enclosing_windows_.find(&window3));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("a"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));

  num_running_apps_ = 0;
  updated_ids_.clear();

  aura::Window window4(nullptr);
  window4.Init(ui::LAYER_NOT_DRAWN);

  MakeInstanceWithWindow("b", &window4);
  MakeInstanceWithWindow("c", &window2, apps::InstanceState::kRunning);

  EXPECT_EQ(1, num_running_apps_);
  EXPECT_EQ(2u, updated_ids_.size());
  EXPECT_NE(updated_enclosing_windows_.end(),
            updated_enclosing_windows_.find(&window2));
  EXPECT_NE(updated_enclosing_windows_.end(),
            updated_enclosing_windows_.find(&window4));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("b"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));

  instance_registry().RemoveObserver(this);

  num_running_apps_ = 0;
  updated_enclosing_windows_.clear();
  updated_ids_.clear();

  aura::Window window5(nullptr);
  window5.Init(ui::LAYER_NOT_DRAWN);
  MakeInstanceWithWindow("f", &window5, apps::InstanceState::kRunning);

  EXPECT_EQ(0, num_running_apps_);
  EXPECT_EQ(0u, updated_enclosing_windows_.size());
  EXPECT_EQ(0u, updated_ids_.size());
}

TEST_F(InstanceRegistryTest, WholeProcessForOneWindow) {
  InstanceRecursiveObserver observer(&instance_registry());

  apps::InstanceState instance_state = apps::InstanceState::kStarted;
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  observer.PrepareForOnInstances(1);
  MakeInstanceWithWindow("p", &window, instance_state);
  // Instance is updated twice, for creation and state modification.
  EXPECT_EQ(1, observer.NumInstancesSeenOnInstanceUpdate());

  instance_state = static_cast<apps::InstanceState>(
      instance_state | apps::InstanceState::kRunning |
      apps::InstanceState::kActive | apps::InstanceState::kVisible);
  observer.PrepareForOnInstances(1);
  MakeInstanceWithWindow("p", &window, instance_state);
  EXPECT_EQ(1, observer.NumInstancesSeenOnInstanceUpdate());

  apps::InstanceState state1 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  apps::InstanceState state2 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  apps::InstanceState state3 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  apps::InstanceState state4 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning |
      apps::InstanceState::kActive | apps::InstanceState::kVisible);
  apps::InstanceState state5 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning |
      apps::InstanceState::kVisible);
  apps::InstanceState state6 = apps::InstanceState::kDestroyed;
  observer.PrepareForOnInstances(1);
  MakeInstanceWithWindow("p", &window, state1);
  MakeInstanceWithWindow("p", &window, state2);
  MakeInstanceWithWindow("p", &window, state3);
  MakeInstanceWithWindow("p", &window, state4);
  MakeInstanceWithWindow("p", &window, state5);
  MakeInstanceWithWindow("p", &window, state6);

  // OnInstanceUpdate is called for state1, because state1 is different with
  // previous instance_state. state2 and state3 is not changed, so
  // OnInstanceUpdate is not called. OnInstanceUpdate is called for state4,
  // state5, and state6 separately, because they are different. So
  // OnInstanceUpdate is called 4 times, for state1, state4, state5, and state6.
  EXPECT_EQ(4, observer.NumInstancesSeenOnInstanceUpdate());
  EXPECT_FALSE(instance_registry().ContainsAppId("p"));

  bool found_window = false;
  EXPECT_FALSE(instance_registry().ForInstancesWithWindow(
      &window, [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_FALSE(found_window);

  observer.PrepareForOnInstances(1);
  MakeInstanceWithWindow("p", &window, state5);
  EXPECT_EQ(1, observer.NumInstancesSeenOnInstanceUpdate());

  found_window = false;
  EXPECT_TRUE(instance_registry().ForInstancesWithWindow(
      &window, [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_TRUE(found_window);
}

TEST_F(InstanceRegistryTest, Recursive) {
  InstanceRecursiveObserver observer(&instance_registry());

  apps::InstanceState instance_state1 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  apps::InstanceState instance_state2 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  observer.PrepareForOnInstances(-1);
  MakeInstanceWithWindow("o", &window1, instance_state1);
  MakeInstanceWithWindow("p", &window2, instance_state2);
  EXPECT_EQ(2, observer.NumInstancesSeenOnInstanceUpdate());

  apps::InstanceState instance_state3 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  apps::InstanceState instance_state4 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  std::vector<apps::InstanceState> latest_state;
  latest_state.push_back(instance_state3);
  latest_state.push_back(instance_state3);
  aura::Window window3(nullptr);
  window3.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window4(nullptr);
  window4.Init(ui::LAYER_NOT_DRAWN);
  observer.PrepareForOnInstances(-1);
  MakeInstanceWithWindow("p", &window2, instance_state3);
  MakeInstanceWithWindow("q", &window3, instance_state4);
  MakeInstanceWithWindow("p", &window4, instance_state3);
  EXPECT_EQ(2, observer.NumInstancesSeenOnInstanceUpdate());

  apps::InstanceState instance_state5 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  apps::InstanceState instance_state6 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  apps::InstanceState instance_state7 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning |
      apps::InstanceState::kActive);

  observer.PrepareForOnInstances(4);
  MakeInstanceWithWindow("p", &window2, instance_state5);
  MakeInstanceWithWindow("p", &window2, instance_state6);
  MakeInstanceWithWindow("p", &window2, instance_state7);
  EXPECT_EQ(1, observer.NumInstancesSeenOnInstanceUpdate());

  apps::InstanceState instance_state8 =
      static_cast<apps::InstanceState>(apps::InstanceState::kDestroyed);
  observer.PrepareForOnInstances(-1);
  MakeInstanceWithWindow("p", &window2, instance_state8);
  MakeInstanceWithWindow("p", &window4, instance_state8);
  MakeInstanceWithWindow("q", &window3, instance_state8);
  MakeInstanceWithWindow("o", &window1, instance_state8);
  EXPECT_EQ(4, observer.NumInstancesSeenOnInstanceUpdate());
  EXPECT_FALSE(instance_registry().ContainsAppId("o"));
  EXPECT_FALSE(instance_registry().ContainsAppId("p"));
  EXPECT_FALSE(instance_registry().ContainsAppId("q"));

  bool found_window = false;
  EXPECT_FALSE(instance_registry().ForInstancesWithWindow(
      &window2, [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_FALSE(found_window);

  found_window = false;
  EXPECT_FALSE(instance_registry().ForInstancesWithWindow(
      &window4, [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_FALSE(found_window);

  found_window = false;
  EXPECT_FALSE(instance_registry().ForInstancesWithWindow(
      &window3, [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_FALSE(found_window);

  found_window = false;
  EXPECT_FALSE(instance_registry().ForInstancesWithWindow(
      &window1, [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_FALSE(found_window);

  observer.PrepareForOnInstances(1);
  MakeInstanceWithWindow("p", &window2, instance_state7);
  EXPECT_EQ(1, observer.NumInstancesSeenOnInstanceUpdate());

  found_window = false;
  EXPECT_TRUE(instance_registry().ForInstancesWithWindow(
      &window2, [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_TRUE(found_window);
}

TEST_F(InstanceRegistryTest, SuperRecursive) {
  InstanceRecursiveObserver observer(&instance_registry());

  // Set up a series of OnInstance to be called during
  // observer.OnInstanceUpdate:
  //  - the 1st update is {'a', &window2, kActive}.
  //  - the 2nd update is {'b', &window3, kActive}.
  //  - the 3rd update is {'c', &window4, kActive}.
  //  - the 4th update is {'b', &window5, kVisible}.
  //  - the 5th update is {'c', &window4, kVisible}.
  //  - the 6td update is {'b', &window3, kRunning}.
  //  - the 7th update is {'a', &window2, kRunning}.
  //  - the 8th update is {'a', &window2, kDestroyed}.
  //  - the 9th update is {'a', &window2, kUnknown}.
  //  - the 10th update is {'a', &window1, kStarted}.
  //
  // The vector is processed in LIFO order with nullptr punctuation to
  // terminate each group. See the comment on the
  // RecursiveObserver::super_recursive_apps_ field.
  std::vector<std::unique_ptr<apps::InstanceParams>> super_recursive_apps;
  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window3(nullptr);
  window3.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window4(nullptr);
  window4.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window5(nullptr);
  window5.Init(ui::LAYER_NOT_DRAWN);
  super_recursive_apps.push_back(nullptr);

  std::unique_ptr<apps::InstanceParams> params =
      std::make_unique<apps::InstanceParams>("a", &window1);
  params->state = std::make_pair(apps::InstanceState::kStarted, base::Time());
  super_recursive_apps.push_back(std::move(params));

  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(
      std::make_unique<apps::InstanceParams>("a", &window2));
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(
      std::make_unique<apps::InstanceParams>("b", &window3));

  params = std::make_unique<apps::InstanceParams>("a", &window2);
  params->state = std::make_pair(apps::InstanceState::kDestroyed, base::Time());
  super_recursive_apps.push_back(std::move(params));

  params = std::make_unique<apps::InstanceParams>("a", &window2);
  params->state = std::make_pair(apps::InstanceState::kRunning, base::Time());
  super_recursive_apps.push_back(std::move(params));

  params = std::make_unique<apps::InstanceParams>("b", &window3);
  params->state = std::make_pair(apps::InstanceState::kRunning, base::Time());
  super_recursive_apps.push_back(std::move(params));

  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(nullptr);

  params = std::make_unique<apps::InstanceParams>("c", &window4);
  params->state = std::make_pair(apps::InstanceState::kVisible, base::Time());
  super_recursive_apps.push_back(std::move(params));

  params = std::make_unique<apps::InstanceParams>("b", &window5);
  params->state = std::make_pair(apps::InstanceState::kVisible, base::Time());
  super_recursive_apps.push_back(std::move(params));

  params = std::make_unique<apps::InstanceParams>("a", &window2);
  params->state = std::make_pair(apps::InstanceState::kActive, base::Time());
  MakeInstanceWithWindow("a", &window2, apps::InstanceState::kActive);

  params = std::make_unique<apps::InstanceParams>("b", &window3);
  params->state = std::make_pair(apps::InstanceState::kActive, base::Time());
  MakeInstanceWithWindow("b", &window3, apps::InstanceState::kActive);

  observer.PrepareParamsForOnInstances(-1, &super_recursive_apps);

  params = std::make_unique<apps::InstanceParams>("c", &window4);
  params->state = std::make_pair(apps::InstanceState::kActive, base::Time());
  MakeInstanceWithWindow("c", &window4, apps::InstanceState::kActive);
  EXPECT_EQ(8, observer.NumInstancesSeenOnInstanceUpdate());

  // After all of that, check that for each window, the last delta won.
  EXPECT_EQ(apps::InstanceState::kStarted,
            instance_registry().GetState(&window1));
  EXPECT_EQ(apps::InstanceState::kUnknown,
            instance_registry().GetState(&window2));
  EXPECT_EQ(apps::InstanceState::kRunning,
            instance_registry().GetState(&window3));
  EXPECT_EQ(apps::InstanceState::kVisible,
            instance_registry().GetState(&window4));
  EXPECT_EQ(apps::InstanceState::kVisible,
            instance_registry().GetState(&window5));

  EXPECT_TRUE(instance_registry().Exists(&window1));
  EXPECT_TRUE(instance_registry().Exists(&window2));
  EXPECT_TRUE(instance_registry().Exists(&window3));
  EXPECT_TRUE(instance_registry().Exists(&window4));
  EXPECT_TRUE(instance_registry().Exists(&window5));
}

TEST_F(InstanceRegistryTest, GetInstances) {
  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window3(nullptr);
  window3.Init(ui::LAYER_NOT_DRAWN);

  MakeInstanceWithWindow("a", &window1);
  MakeInstanceWithWindow("b", &window2);
  MakeInstanceWithWindow("a", &window3);

  auto instances = instance_registry().GetInstances("a");
  EXPECT_EQ(2U, instances.size());

  std::set<aura::Window*> windows;
  for (const apps::Instance* instance : instances) {
    EXPECT_EQ("a", instance->AppId());
    windows.insert(instance->Window());
  }
  EXPECT_TRUE(base::Contains(windows, &window1));
  EXPECT_TRUE(base::Contains(windows, &window3));

  instances = instance_registry().GetInstances("b");
  EXPECT_EQ(1U, instances.size());

  auto it = instances.begin();
  EXPECT_EQ("b", (*it)->AppId());
  EXPECT_EQ(&window2, (*it)->Window());

  MakeInstanceWithWindow("a", &window1, apps::InstanceState::kDestroyed);
  MakeInstanceWithWindow("b", &window2, apps::InstanceState::kDestroyed);

  instances = instance_registry().GetInstances("a");
  EXPECT_EQ(1U, instances.size());

  it = instances.begin();
  EXPECT_EQ("a", (*it)->AppId());
  EXPECT_EQ(&window3, (*it)->Window());

  instances = instance_registry().GetInstances("b");
  EXPECT_TRUE(instances.empty());
}

TEST_F(InstanceRegistryTest, ContainsAppId) {
  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window3(nullptr);
  window3.Init(ui::LAYER_NOT_DRAWN);

  MakeInstanceWithWindow("a", &window1);
  MakeInstanceWithWindow("b", &window2);
  MakeInstanceWithWindow("a", &window3);

  EXPECT_TRUE(instance_registry().ContainsAppId("a"));
  EXPECT_TRUE(instance_registry().ContainsAppId("b"));
  EXPECT_FALSE(instance_registry().ContainsAppId("c"));

  MakeInstanceWithWindow("a", &window1, apps::InstanceState::kDestroyed);
  MakeInstanceWithWindow("b", &window2, apps::InstanceState::kDestroyed);

  EXPECT_TRUE(instance_registry().ContainsAppId("a"));
  EXPECT_FALSE(instance_registry().ContainsAppId("b"));
}

TEST_F(InstanceRegistryTest, WindowIsChanged) {
  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);

  auto instance_id1 = base::UnguessableToken::Create();
  auto delta = std::make_unique<apps::Instance>("a", instance_id1, &window1);
  instance_registry().OnInstance(std::move(delta));
  EXPECT_TRUE(instance_registry().Exists(&window1));
  EXPECT_EQ(1U, instance_registry().GetInstances("a").size());

  // Modify window for the instance of `a`.
  delta = std::make_unique<apps::Instance>("a", instance_id1, &window2);
  instance_registry().OnInstance(std::move(delta));
  EXPECT_FALSE(instance_registry().Exists(&window1));
  EXPECT_TRUE(instance_registry().Exists(&window2));
  EXPECT_EQ(1U, instance_registry().GetInstances("a").size());

  // Create an instance of `window2` for `b`.
  auto instance_id2 = base::UnguessableToken::Create();
  delta = std::make_unique<apps::Instance>("b", instance_id2, &window2);
  instance_registry().OnInstance(std::move(delta));
  EXPECT_TRUE(instance_registry().Exists(&window2));
  EXPECT_EQ(1U, instance_registry().GetInstances("b").size());

  // Close window for the instance of `a`.
  delta = std::make_unique<apps::Instance>("a", instance_id1, &window2);
  delta->UpdateState(apps::InstanceState::kDestroyed, base::Time());
  instance_registry().OnInstance(std::move(delta));
  EXPECT_TRUE(instance_registry().Exists(&window2));
  EXPECT_TRUE(instance_registry().GetInstances("a").empty());

  // Close window for the instance of `b`.
  delta = std::make_unique<apps::Instance>("b", instance_id2, &window2);
  delta->UpdateState(apps::InstanceState::kDestroyed, base::Time());
  instance_registry().OnInstance(std::move(delta));
  EXPECT_FALSE(instance_registry().Exists(&window2));
  EXPECT_TRUE(instance_registry().GetInstances("b").empty());
}

TEST_F(InstanceRegistryTest, SuperRecursiveWithWindowChanged) {
  InstanceRecursiveObserver observer(&instance_registry());

  // Set up a series of OnInstance to be called during
  // observer.OnInstanceUpdate:
  //  - the 1st update is {'a', id1, &window2, kActive}.
  //  - the 2nd update is {'b', id2, &window3, kActive}.
  //  - the 3rd update is {'c', id3, &window4, kActive}.
  //  - the 4th update is {'b', id4, &window3, kVisible}.
  //  - the 5th update is {'c', id3, &window3, kVisible}.
  //  - the 6td update is {'b', id2, &window3, kDestroyed}.
  //  - the 7th update is {'a', id1, &window5, kRunning}.
  //  - the 8th update is {'a', id1, &window5, kDestroyed}.
  //  - the 9th update is {'a', id5, &window2, kUnknown}.
  //  - the 10th update is {'a', id6, &window1, kStarted}.
  //
  // The vector is processed in LIFO order with nullptr punctuation to
  // terminate each group. See the comment on the
  // RecursiveObserver::super_recursive_apps_ field.
  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window3(nullptr);
  window3.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window4(nullptr);
  window4.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window5(nullptr);
  window5.Init(ui::LAYER_NOT_DRAWN);

  auto instance_id1 = base::UnguessableToken::Create();
  auto instance_id2 = base::UnguessableToken::Create();
  auto instance_id3 = base::UnguessableToken::Create();
  auto instance_id4 = base::UnguessableToken::Create();
  auto instance_id5 = base::UnguessableToken::Create();
  auto instance_id6 = base::UnguessableToken::Create();

  std::vector<std::unique_ptr<apps::Instance>> super_recursive_apps;
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(nullptr);

  // the 10th update is {'a', id6, &window1, kStarted}.
  auto delta = std::make_unique<apps::Instance>("a", instance_id6, &window1);
  delta->UpdateState(apps::InstanceState::kStarted, base::Time());
  super_recursive_apps.push_back(std::move(delta));

  delta = std::make_unique<apps::Instance>("a", instance_id6, &window1);
  delta->UpdateState(apps::InstanceState::kDestroyed, base::Time());
  super_recursive_apps.push_back(std::move(delta));
  super_recursive_apps.push_back(nullptr);

  // The 9th update is {'a', id5, &window2, kUnknown}.
  super_recursive_apps.push_back(
      std::make_unique<apps::Instance>("a", instance_id5, &window2));

  // The 8th update is {'a', id1, &window5, kDestroyed}.
  delta = std::make_unique<apps::Instance>("a", instance_id1, &window5);
  delta->UpdateState(apps::InstanceState::kDestroyed, base::Time());
  super_recursive_apps.push_back(std::move(delta));

  // The 7th update is {'a', id1, &window5, kRunning}.
  delta = std::make_unique<apps::Instance>("a", instance_id1, &window5);
  delta->UpdateState(apps::InstanceState::kRunning, base::Time());
  super_recursive_apps.push_back(std::move(delta));

  // The 6td update is {'b', id2, &window3, kDestroyed}.
  delta = std::make_unique<apps::Instance>("b", instance_id2, &window3);
  delta->UpdateState(apps::InstanceState::kDestroyed, base::Time());
  super_recursive_apps.push_back(std::move(delta));

  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(nullptr);

  // The 5th update is {'c', id3, &window3, kVisible}.
  delta = std::make_unique<apps::Instance>("c", instance_id3, &window3);
  delta->UpdateState(apps::InstanceState::kVisible, base::Time());
  super_recursive_apps.push_back(std::move(delta));

  // The 4th update is {'b', id4, &window3, kVisible}.
  delta = std::make_unique<apps::Instance>("b", instance_id4, &window3);
  delta->UpdateState(apps::InstanceState::kVisible, base::Time());
  super_recursive_apps.push_back(std::move(delta));

  // The 1st update is {'a', id1, &window2, kActive}
  delta = std::make_unique<apps::Instance>("a", instance_id1, &window2);
  delta->UpdateState(apps::InstanceState::kActive, base::Time());
  instance_registry().OnInstance(std::move(delta));

  // The 2nd update is {'b', id2, &window3, kActive}.
  delta = std::make_unique<apps::Instance>("b", instance_id2, &window3);
  delta->UpdateState(apps::InstanceState::kActive, base::Time());
  instance_registry().OnInstance(std::move(delta));

  // The 3rd update is {'c', id3, &window4, kActive}.
  delta = std::make_unique<apps::Instance>("c", instance_id3, &window4);
  delta->UpdateState(apps::InstanceState::kActive, base::Time());
  observer.PrepareForOnInstances(-1, &super_recursive_apps);
  instance_registry().OnInstance(std::move(delta));
  EXPECT_EQ(8, observer.NumInstancesSeenOnInstanceUpdate());

  // After all of that, check that for each window, the last delta won.
  auto states = GetStates("a", &window1);
  EXPECT_EQ(1U, states.size());
  EXPECT_EQ(apps::InstanceState::kStarted, *states.begin());

  states = GetStates("a", &window2);
  EXPECT_EQ(1U, states.size());
  EXPECT_EQ(apps::InstanceState::kUnknown, *states.begin());

  EXPECT_EQ(2U, instance_registry().GetInstances("a").size());

  states = GetStates("b", &window3);
  EXPECT_EQ(1U, states.size());
  EXPECT_EQ(apps::InstanceState::kVisible, *states.begin());

  EXPECT_EQ(1U, instance_registry().GetInstances("b").size());

  states = GetStates("c", &window3);
  EXPECT_EQ(1U, states.size());
  EXPECT_EQ(apps::InstanceState::kVisible, *states.begin());

  EXPECT_EQ(1U, instance_registry().GetInstances("c").size());

  EXPECT_TRUE(GetStates("c", &window4).empty());

  EXPECT_TRUE(GetStates("a", &window5).empty());

  EXPECT_TRUE(instance_registry().Exists(&window1));
  EXPECT_TRUE(instance_registry().Exists(&window2));
  EXPECT_TRUE(instance_registry().Exists(&window3));
  EXPECT_FALSE(instance_registry().Exists(&window4));
  EXPECT_FALSE(instance_registry().Exists(&window5));
}
