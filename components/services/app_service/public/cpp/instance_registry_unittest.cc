// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <vector>

#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

class InstanceRegistryTest : public testing::Test,
                             public apps::InstanceRegistry::Observer {
 protected:
  static std::unique_ptr<apps::Instance> MakeInstance(
      const char* app_id,
      aura::Window* window,
      apps::InstanceState state = apps::InstanceState::kUnknown,
      base::Time time = base::Time()) {
    std::unique_ptr<apps::Instance> instance = std::make_unique<apps::Instance>(
        app_id, apps::Instance::InstanceKey(window));
    instance->UpdateState(state, time);
    return instance;
  }

  static apps::Instance::InstanceKey MakeInstanceKey(aura::Window* window) {
    return apps::Instance::InstanceKey(window);
  }

  void CallForEachInstance(apps::InstanceRegistry& instance_registry) {
    instance_registry.ForEachInstance(
        [this](const apps::InstanceUpdate& update) {
          OnInstanceUpdate(update);
        });
  }

  apps::InstanceState GetState(apps::InstanceRegistry& instance_registry,
                               aura::Window* window) {
    return instance_registry.GetState(apps::Instance::InstanceKey(window));
  }

  // apps::InstanceRegistry::Observer overrides.
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override {
    EXPECT_NE("", update.AppId());
    if (update.StateChanged() &&
        update.State() == apps::InstanceState::kRunning) {
      num_running_apps_++;
    }
    updated_ids_.insert(update.AppId());
    updated_windows_.insert(update.Window());
  }

  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* instance_registry) override {
    // The test code explicitly calls both AddObserver and RemoveObserver.
    NOTREACHED();
  }

  int num_running_apps_ = 0;
  std::set<std::string> updated_ids_;
  std::set<const aura::Window*> updated_windows_;
};

// In the tests below, just "recursive" means that instance_registry.OnInstances
// calls observer.OnInstanceUpdate which calls instance_registry.ForEachInstance
// and instance_registry.ForOneInstance. "Super-recursive" means that
// instance_registry.OnInstances calls observer.OnInstanceUpdate calls
// instance_registry.OnInstances which calls observer.OnInstanceUpdate.
class InstanceRecursiveObserver : public apps::InstanceRegistry::Observer {
 public:
  explicit InstanceRecursiveObserver(apps::InstanceRegistry* instance_registry)
      : instance_registry_(instance_registry) {
    Observe(instance_registry);
  }

  ~InstanceRecursiveObserver() override = default;

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
          if (outer.Window() == inner.Window()) {
            ExpectEq(outer, inner);
          }
          num_instance++;
        });

    EXPECT_TRUE(instance_registry_->ForOneInstance(
        apps::Instance::InstanceKey(outer.Window()),
        [&outer](const apps::InstanceUpdate& inner) {
          ExpectEq(outer, inner);
        }));

    if (expected_num_instances_ >= 0) {
      EXPECT_EQ(expected_num_instances_, num_instance);
    }

    std::vector<std::unique_ptr<apps::Instance>> super_recursive;
    while (!super_recursive_instances_.empty()) {
      std::unique_ptr<apps::Instance> instance =
          std::move(super_recursive_instances_.back());
      if (instance.get() == nullptr) {
        // This is the placeholder 'punctuation'.
        super_recursive_instances_.pop_back();
        break;
      }
      super_recursive.push_back(std::move(instance));
      super_recursive_instances_.pop_back();
    }
    if (!super_recursive.empty()) {
      instance_registry_->OnInstances(std::move(super_recursive));
    }

    num_instances_seen_on_instance_update_++;
  }

  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* instance_registry) override {
    Observe(nullptr);
  }

  static void ExpectEq(const apps::InstanceUpdate& outer,
                       const apps::InstanceUpdate& inner) {
    EXPECT_EQ(outer.AppId(), inner.AppId());
    EXPECT_EQ(outer.Window(), inner.Window());
    EXPECT_EQ(outer.LaunchId(), inner.LaunchId());
    EXPECT_EQ(outer.State(), inner.State());
    EXPECT_EQ(outer.LastUpdatedTime(), inner.LastUpdatedTime());
    EXPECT_EQ(outer.BrowserContext(), inner.BrowserContext());
  }

  apps::InstanceRegistry* instance_registry_;
  int expected_num_instances_;
  int num_instances_seen_on_instance_update_;

  // Non-empty when this.OnInstanceUpdate should trigger more
  // instance_registry_.OnInstances calls.
  //
  // During OnInstanceUpdate, this vector (a stack) is popped from the back
  // until a nullptr 'punctuation' element (a group terminator) is seen. If that
  // group of popped elements (in LIFO order) is non-empty, that group forms the
  // vector of App's passed to instance_registry_.OnInstances.
  std::vector<std::unique_ptr<apps::Instance>> super_recursive_instances_;
};

TEST_F(InstanceRegistryTest, ForEachInstance) {
  std::vector<std::unique_ptr<apps::Instance>> deltas;
  apps::InstanceRegistry instance_registry;

  updated_windows_.clear();
  updated_ids_.clear();

  CallForEachInstance(instance_registry);

  EXPECT_EQ(0u, updated_windows_.size());
  EXPECT_EQ(0u, updated_ids_.size());

  deltas.clear();
  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window3(nullptr);
  window3.Init(ui::LAYER_NOT_DRAWN);
  deltas.push_back(MakeInstance("a", &window1));
  deltas.push_back(MakeInstance("b", &window2));
  deltas.push_back(MakeInstance("c", &window3));
  instance_registry.OnInstances(std::move(deltas));
  EXPECT_TRUE(instance_registry.GetWindows("a") ==
              std::set<aura::Window*>{&window1});
  EXPECT_TRUE(instance_registry.GetWindows("b") ==
              std::set<aura::Window*>{&window2});
  EXPECT_TRUE(instance_registry.GetWindows("c") ==
              std::set<aura::Window*>{&window3});

  updated_windows_.clear();
  updated_ids_.clear();
  CallForEachInstance(instance_registry);

  EXPECT_EQ(3u, updated_windows_.size());
  EXPECT_EQ(3u, updated_ids_.size());
  EXPECT_NE(updated_windows_.end(), updated_windows_.find(&window1));
  EXPECT_NE(updated_windows_.end(), updated_windows_.find(&window2));
  EXPECT_NE(updated_windows_.end(), updated_windows_.find(&window3));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("a"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("b"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));

  deltas.clear();
  aura::Window window4(nullptr);
  window4.Init(ui::LAYER_NOT_DRAWN);
  deltas.push_back(MakeInstance("a", &window1, apps::InstanceState::kRunning));
  deltas.push_back(MakeInstance("c", &window4));
  instance_registry.OnInstances(std::move(deltas));
  EXPECT_TRUE(instance_registry.GetWindows("a") ==
              std::set<aura::Window*>{&window1});
  EXPECT_TRUE(instance_registry.GetWindows("c") ==
              (std::set<aura::Window*>{&window3, &window4}));

  updated_windows_.clear();
  updated_ids_.clear();
  CallForEachInstance(instance_registry);

  EXPECT_EQ(4u, updated_windows_.size());
  EXPECT_EQ(3u, updated_ids_.size());
  EXPECT_NE(updated_windows_.end(), updated_windows_.find(&window1));
  EXPECT_NE(updated_windows_.end(), updated_windows_.find(&window2));
  EXPECT_NE(updated_windows_.end(), updated_windows_.find(&window3));
  EXPECT_NE(updated_windows_.end(), updated_windows_.find(&window4));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("a"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("b"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));

  // Test that ForOneApp succeeds for window4 and fails for window5.

  bool found_window4 = false;
  EXPECT_TRUE(instance_registry.ForOneInstance(
      apps::Instance::InstanceKey(&window4),
      [&found_window4](const apps::InstanceUpdate& update) {
        found_window4 = true;
        EXPECT_EQ("c", update.AppId());
      }));
  EXPECT_TRUE(found_window4);

  bool found_window5 = false;
  aura::Window window5(nullptr);
  window5.Init(ui::LAYER_NOT_DRAWN);
  EXPECT_FALSE(instance_registry.ForOneInstance(
      apps::Instance::InstanceKey(&window5),
      [&found_window5](const apps::InstanceUpdate& update) {
        found_window5 = true;
      }));
  EXPECT_FALSE(found_window5);
}

TEST_F(InstanceRegistryTest, Observer) {
  std::vector<std::unique_ptr<apps::Instance>> deltas;
  apps::InstanceRegistry instance_registry;

  instance_registry.AddObserver(this);

  num_running_apps_ = 0;
  updated_windows_.clear();
  updated_ids_.clear();
  deltas.clear();

  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window3(nullptr);
  window3.Init(ui::LAYER_NOT_DRAWN);

  deltas.push_back(MakeInstance("a", &window1));
  deltas.push_back(MakeInstance("c", &window2));
  deltas.push_back(MakeInstance("a", &window3));
  instance_registry.OnInstances(std::move(deltas));

  EXPECT_EQ(0, num_running_apps_);
  EXPECT_EQ(3u, updated_windows_.size());
  EXPECT_EQ(2u, updated_ids_.size());
  EXPECT_NE(updated_windows_.end(), updated_windows_.find(&window1));
  EXPECT_NE(updated_windows_.end(), updated_windows_.find(&window2));
  EXPECT_NE(updated_windows_.end(), updated_windows_.find(&window3));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("a"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));

  num_running_apps_ = 0;
  updated_ids_.clear();
  deltas.clear();

  aura::Window window4(nullptr);
  window4.Init(ui::LAYER_NOT_DRAWN);

  deltas.push_back(MakeInstance("b", &window4));
  deltas.push_back(MakeInstance("c", &window2, apps::InstanceState::kRunning));
  instance_registry.OnInstances(std::move(deltas));

  EXPECT_EQ(1, num_running_apps_);
  EXPECT_EQ(2u, updated_ids_.size());
  EXPECT_NE(updated_windows_.end(), updated_windows_.find(&window2));
  EXPECT_NE(updated_windows_.end(), updated_windows_.find(&window4));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("b"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));

  instance_registry.RemoveObserver(this);

  num_running_apps_ = 0;
  updated_windows_.clear();
  updated_ids_.clear();
  deltas.clear();

  aura::Window window5(nullptr);
  window5.Init(ui::LAYER_NOT_DRAWN);
  deltas.push_back(MakeInstance("f", &window5, apps::InstanceState::kRunning));
  instance_registry.OnInstances(std::move(deltas));

  EXPECT_EQ(0, num_running_apps_);
  EXPECT_EQ(0u, updated_windows_.size());
  EXPECT_EQ(0u, updated_ids_.size());
}

TEST_F(InstanceRegistryTest, WholeProcessForOneWindow) {
  std::vector<std::unique_ptr<apps::Instance>> deltas;
  apps::InstanceRegistry instance_registry;
  InstanceRecursiveObserver observer(&instance_registry);

  apps::InstanceState instance_state = apps::InstanceState::kStarted;
  deltas.clear();
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  observer.PrepareForOnInstances(1);
  deltas.push_back(MakeInstance("p", &window, instance_state));
  instance_registry.OnInstances(std::move(deltas));
  EXPECT_EQ(1, observer.NumInstancesSeenOnInstanceUpdate());

  instance_state = static_cast<apps::InstanceState>(
      instance_state | apps::InstanceState::kRunning |
      apps::InstanceState::kActive | apps::InstanceState::kVisible);
  observer.PrepareForOnInstances(1);
  deltas.clear();
  deltas.push_back(MakeInstance("p", &window, instance_state));
  instance_registry.OnInstances(std::move(deltas));
  EXPECT_EQ(1, observer.NumInstancesSeenOnInstanceUpdate());
  EXPECT_TRUE(instance_registry.GetWindows("p") ==
              std::set<aura::Window*>{&window});

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
  deltas.clear();
  deltas.push_back(MakeInstance("p", &window, state1));
  deltas.push_back(MakeInstance("p", &window, state2));
  deltas.push_back(MakeInstance("p", &window, state3));
  deltas.push_back(MakeInstance("p", &window, state4));
  deltas.push_back(MakeInstance("p", &window, state5));
  deltas.push_back(MakeInstance("p", &window, state6));
  instance_registry.OnInstances(std::move(deltas));
  // OnInstanceUpdate is called for state1, because state1 is different with
  // previous instance_state. state2 and state3 is not changed, so
  // OnInstanceUpdate is not called. OnInstanceUpdate is called for state4,
  // state5, and state6 separately, because they are different. So
  // OnInstanceUpdate is called 4 times, for state1, state4, state5, and state6.
  EXPECT_EQ(4, observer.NumInstancesSeenOnInstanceUpdate());
  EXPECT_TRUE(instance_registry.GetWindows("p").empty());

  bool found_window = false;
  EXPECT_FALSE(instance_registry.ForOneInstance(
      apps::Instance::InstanceKey(&window),
      [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_FALSE(found_window);

  observer.PrepareForOnInstances(1);
  deltas.clear();
  deltas.push_back(MakeInstance("p", &window, state5));
  instance_registry.OnInstances(std::move(deltas));
  EXPECT_EQ(1, observer.NumInstancesSeenOnInstanceUpdate());
  EXPECT_TRUE(instance_registry.GetWindows("p") ==
              std::set<aura::Window*>{&window});

  found_window = false;
  EXPECT_TRUE(instance_registry.ForOneInstance(
      apps::Instance::InstanceKey(&window),
      [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_TRUE(found_window);
}

TEST_F(InstanceRegistryTest, Recursive) {
  std::vector<std::unique_ptr<apps::Instance>> deltas;
  apps::InstanceRegistry instance_registry;
  InstanceRecursiveObserver observer(&instance_registry);

  apps::InstanceState instance_state1 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  apps::InstanceState instance_state2 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  deltas.clear();
  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  observer.PrepareForOnInstances(-1);
  deltas.push_back(MakeInstance("o", &window1, instance_state1));
  deltas.push_back(MakeInstance("p", &window2, instance_state2));
  instance_registry.OnInstances(std::move(deltas));
  EXPECT_EQ(2, observer.NumInstancesSeenOnInstanceUpdate());
  EXPECT_TRUE(instance_registry.GetWindows("o") ==
              std::set<aura::Window*>{&window1});
  EXPECT_TRUE(instance_registry.GetWindows("p") ==
              std::set<aura::Window*>{&window2});

  apps::InstanceState instance_state3 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  apps::InstanceState instance_state4 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  std::vector<apps::InstanceState> latest_state;
  latest_state.push_back(instance_state3);
  latest_state.push_back(instance_state3);
  deltas.clear();
  aura::Window window3(nullptr);
  window3.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window4(nullptr);
  window4.Init(ui::LAYER_NOT_DRAWN);
  observer.PrepareForOnInstances(-1);
  deltas.push_back(MakeInstance("p", &window2, instance_state3));
  deltas.push_back(MakeInstance("q", &window3, instance_state4));
  deltas.push_back(MakeInstance("p", &window4, instance_state3));
  instance_registry.OnInstances(std::move(deltas));
  EXPECT_EQ(2, observer.NumInstancesSeenOnInstanceUpdate());
  EXPECT_TRUE(instance_registry.GetWindows("p") ==
              (std::set<aura::Window*>{&window2, &window4}));
  EXPECT_TRUE(instance_registry.GetWindows("q") ==
              std::set<aura::Window*>{&window3});

  apps::InstanceState instance_state5 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  apps::InstanceState instance_state6 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  apps::InstanceState instance_state7 = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning |
      apps::InstanceState::kActive);

  observer.PrepareForOnInstances(4);
  deltas.clear();
  deltas.push_back(MakeInstance("p", &window2, instance_state5));
  deltas.push_back(MakeInstance("p", &window2, instance_state6));
  deltas.push_back(MakeInstance("p", &window2, instance_state7));
  instance_registry.OnInstances(std::move(deltas));
  EXPECT_EQ(1, observer.NumInstancesSeenOnInstanceUpdate());
  EXPECT_TRUE(instance_registry.GetWindows("p") ==
              (std::set<aura::Window*>{&window2, &window4}));

  apps::InstanceState instance_state8 =
      static_cast<apps::InstanceState>(apps::InstanceState::kDestroyed);
  observer.PrepareForOnInstances(-1);
  deltas.clear();
  deltas.push_back(MakeInstance("p", &window2, instance_state8));
  deltas.push_back(MakeInstance("p", &window4, instance_state8));
  deltas.push_back(MakeInstance("q", &window3, instance_state8));
  deltas.push_back(MakeInstance("o", &window1, instance_state8));
  instance_registry.OnInstances(std::move(deltas));
  EXPECT_EQ(4, observer.NumInstancesSeenOnInstanceUpdate());
  EXPECT_TRUE(instance_registry.GetWindows("o").empty());
  EXPECT_TRUE(instance_registry.GetWindows("p").empty());
  EXPECT_TRUE(instance_registry.GetWindows("q").empty());

  bool found_window = false;
  EXPECT_FALSE(instance_registry.ForOneInstance(
      apps::Instance::InstanceKey(&window2),
      [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_FALSE(found_window);

  found_window = false;
  EXPECT_FALSE(instance_registry.ForOneInstance(
      apps::Instance::InstanceKey(&window4),
      [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_FALSE(found_window);

  found_window = false;
  EXPECT_FALSE(instance_registry.ForOneInstance(
      apps::Instance::InstanceKey(&window3),
      [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_FALSE(found_window);

  found_window = false;
  EXPECT_FALSE(instance_registry.ForOneInstance(
      apps::Instance::InstanceKey(&window1),
      [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_FALSE(found_window);

  observer.PrepareForOnInstances(1);
  deltas.clear();
  deltas.push_back(MakeInstance("p", &window2, instance_state7));
  instance_registry.OnInstances(std::move(deltas));
  EXPECT_EQ(1, observer.NumInstancesSeenOnInstanceUpdate());
  EXPECT_TRUE(instance_registry.GetWindows("p") ==
              std::set<aura::Window*>{&window2});

  found_window = false;
  EXPECT_TRUE(instance_registry.ForOneInstance(
      apps::Instance::InstanceKey(&window2),
      [&found_window](const apps::InstanceUpdate& update) {
        found_window = true;
      }));
  EXPECT_TRUE(found_window);
}

TEST_F(InstanceRegistryTest, SuperRecursive) {
  std::vector<std::unique_ptr<apps::Instance>> deltas;
  apps::InstanceRegistry instance_registry;
  InstanceRecursiveObserver observer(&instance_registry);

  // Set up a series of OnInstances to be called during
  // observer.OnInstanceUpdate:
  //  - the 1st update is {'a', &window2, kActive}.
  //  - the 2nd update is {'b', &window3, kActive}.
  //  - the 3rd update is {'c', &window4, kActive}.
  //  - the 4th update is {'b', &window5, kVisible}.
  //  - the 5th update is {'c', &window4, kVisible}.
  //  - the 6td update is {'b', &window3, kRunning}.
  //  - the 7th update is {'a', &window2, kRunning}.
  //  - the 8th update is {'b', &window1, kStarted}.
  //
  // The vector is processed in LIFO order with nullptr punctuation to
  // terminate each group. See the comment on the
  // RecursiveObserver::super_recursive_apps_ field.
  std::vector<std::unique_ptr<apps::Instance>> super_recursive_apps;
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
  super_recursive_apps.push_back(
      MakeInstance("a", &window1, apps::InstanceState::kStarted));
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(MakeInstance("a", &window2));
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(MakeInstance("b", &window3));
  super_recursive_apps.push_back(
      MakeInstance("a", &window2, apps::InstanceState::kDestroyed));
  super_recursive_apps.push_back(
      MakeInstance("a", &window2, apps::InstanceState::kRunning));
  super_recursive_apps.push_back(
      MakeInstance("b", &window3, apps::InstanceState::kRunning));
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(
      MakeInstance("c", &window4, apps::InstanceState::kVisible));
  super_recursive_apps.push_back(
      MakeInstance("b", &window5, apps::InstanceState::kVisible));

  observer.PrepareForOnInstances(-1, &super_recursive_apps);
  deltas.clear();
  deltas.push_back(MakeInstance("a", &window2, apps::InstanceState::kActive));
  deltas.push_back(MakeInstance("b", &window3, apps::InstanceState::kActive));
  deltas.push_back(MakeInstance("c", &window4, apps::InstanceState::kActive));
  instance_registry.OnInstances(std::move(deltas));
  EXPECT_EQ(10, observer.NumInstancesSeenOnInstanceUpdate());
  EXPECT_TRUE(instance_registry.GetWindows("a") ==
              (std::set<aura::Window*>{&window1, &window2}));
  EXPECT_TRUE(instance_registry.GetWindows("b") ==
              (std::set<aura::Window*>{&window3, &window5}));
  EXPECT_TRUE(instance_registry.GetWindows("c") ==
              std::set<aura::Window*>{&window4});

  // After all of that, check that for each window, the last delta won.
  EXPECT_EQ(apps::InstanceState::kStarted,
            GetState(instance_registry, &window1));
  EXPECT_EQ(apps::InstanceState::kUnknown,
            GetState(instance_registry, &window2));
  EXPECT_EQ(apps::InstanceState::kRunning,
            GetState(instance_registry, &window3));
  EXPECT_EQ(apps::InstanceState::kVisible,
            GetState(instance_registry, &window4));
  EXPECT_EQ(apps::InstanceState::kVisible,
            GetState(instance_registry, &window5));
}

TEST_F(InstanceRegistryTest, GetInstanceKeys) {
  std::vector<std::unique_ptr<apps::Instance>> deltas;
  apps::InstanceRegistry instance_registry;

  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window3(nullptr);
  window3.Init(ui::LAYER_NOT_DRAWN);

  deltas.push_back(MakeInstance("a", &window1));
  deltas.push_back(MakeInstance("b", &window2));
  deltas.push_back(MakeInstance("a", &window3));
  instance_registry.OnInstances(deltas);

  EXPECT_TRUE(instance_registry.GetInstanceKeys("a") ==
              (std::set<const apps::Instance::InstanceKey>{
                  MakeInstanceKey(&window1), MakeInstanceKey(&window3)}));
  EXPECT_TRUE(
      instance_registry.GetInstanceKeys("b") ==
      (std::set<const apps::Instance::InstanceKey>{MakeInstanceKey(&window2)}));

  deltas.clear();
  deltas.push_back(
      MakeInstance("a", &window1, apps::InstanceState::kDestroyed));
  deltas.push_back(
      MakeInstance("b", &window2, apps::InstanceState::kDestroyed));
  instance_registry.OnInstances(deltas);

  EXPECT_TRUE(
      instance_registry.GetInstanceKeys("a") ==
      (std::set<const apps::Instance::InstanceKey>{MakeInstanceKey(&window3)}));
  EXPECT_TRUE(instance_registry.GetInstanceKeys("b") ==
              (std::set<const apps::Instance::InstanceKey>()));
}

TEST_F(InstanceRegistryTest, ContainsAppId) {
  std::vector<std::unique_ptr<apps::Instance>> deltas;
  apps::InstanceRegistry instance_registry;

  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window3(nullptr);
  window3.Init(ui::LAYER_NOT_DRAWN);

  deltas.push_back(MakeInstance("a", &window1));
  deltas.push_back(MakeInstance("b", &window2));
  deltas.push_back(MakeInstance("a", &window3));
  instance_registry.OnInstances(deltas);

  EXPECT_TRUE(instance_registry.ContainsAppId("a"));
  EXPECT_TRUE(instance_registry.ContainsAppId("b"));
  EXPECT_FALSE(instance_registry.ContainsAppId("c"));

  deltas.clear();
  deltas.push_back(
      MakeInstance("a", &window1, apps::InstanceState::kDestroyed));
  deltas.push_back(
      MakeInstance("b", &window2, apps::InstanceState::kDestroyed));
  instance_registry.OnInstances(deltas);

  EXPECT_TRUE(instance_registry.ContainsAppId("a"));
  EXPECT_FALSE(instance_registry.ContainsAppId("b"));
}