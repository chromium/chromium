// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/arc_resize_lock_manager.h"

#include <set>
#include <string>

#include "ash/constants/app_types.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/resize_shadow_type.h"
#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/arc/compat_mode/metrics.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/class_property.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace arc {
namespace {

class TestArcResizeLockPrefDelegate : public ArcResizeLockPrefDelegate {
 public:
  ~TestArcResizeLockPrefDelegate() override = default;

  // ArcResizeLockPrefDelegate:
  mojom::ArcResizeLockState GetResizeLockState(
      const std::string& app_id) const override {
    auto it = resize_lock_states.find(app_id);
    if (it == resize_lock_states.end())
      return mojom::ArcResizeLockState::UNDEFINED;

    return it->second;
  }
  void SetResizeLockState(const std::string& app_id,
                          mojom::ArcResizeLockState state) override {
    resize_lock_states[app_id] = state;
  }
  bool GetResizeLockNeedsConfirmation(const std::string& app_id) override {
    return false;
  }
  void SetResizeLockNeedsConfirmation(const std::string& app_id,
                                      bool is_needed) override {}

 private:
  base::flat_map<std::string, mojom::ArcResizeLockState> resize_lock_states;
  int GetShowSplashScreenDialogCount() const override { return 1; }
  void SetShowSplashScreenDialogCount(int count) override {}
};

class ScopedWindowPropertyObserver : public aura::WindowObserver {
 public:
  using WindowPropertyChangedCallback =
      base::RepeatingCallback<void(aura::Window*, const void*, intptr_t)>;

  ScopedWindowPropertyObserver(aura::Window* window,
                               WindowPropertyChangedCallback on_changed)
      : on_changed_(std::move(on_changed)) {
    observer_.Observe(window);
  }
  ~ScopedWindowPropertyObserver() override { observer_.Reset(); }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    on_changed_.Run(window, key, old);
  }
  void OnWindowDestroying(aura::Window* window) override { observer_.Reset(); }

 private:
  WindowPropertyChangedCallback on_changed_;
  base::ScopedObservation<aura::Window, aura::WindowObserver> observer_{this};
};

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kNonInterestedPropKey, false)

}  // namespace

class ArcResizeLockManagerTest : public views::ViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    arc_resize_lock_manager_.SetPrefDelegate(
        &test_arc_resize_lock_pref_delegate);
  }

  aura::Window* CreateFakeWindow(bool is_arc) {
    aura::Window* window =
        new aura::Window(nullptr, aura::client::WINDOW_TYPE_NORMAL);
    if (is_arc) {
      window->SetProperty(aura::client::kAppType,
                          static_cast<int>(ash::AppType::ARC_APP));
    }
    window->Init(ui::LAYER_TEXTURED);
    window->Show();
    return window;
  }

  bool IsResizeLockEnabled(aura::Window* window) const {
    return arc_resize_lock_manager_.resize_lock_enabled_windows_.contains(
        window);
  }

  TestArcResizeLockPrefDelegate* pref_delegate() {
    return &test_arc_resize_lock_pref_delegate;
  }

 private:
  ArcResizeLockManager arc_resize_lock_manager_{nullptr, nullptr};
  TestArcResizeLockPrefDelegate test_arc_resize_lock_pref_delegate;
};

TEST_F(ArcResizeLockManagerTest, ConstructDestruct) {}

// Tests that resize lock state is properly sync'ed with the window property.
TEST_F(ArcResizeLockManagerTest, TestPropertyChange) {
  auto* arc_window = CreateFakeWindow(true);

  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // App id needs to be set to toogle resize lock state.
  arc_window->SetProperty(ash::kAppIDKey, new std::string("app-id"));
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // Test EnableResizeLock will be called by the property change.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_LIMITED);
  EXPECT_TRUE(IsResizeLockEnabled(arc_window));

  // Test nothing will be called by the property overwrite with the same value.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_LIMITED);
  EXPECT_TRUE(IsResizeLockEnabled(arc_window));

  // Test DisableResizeLock will be called by the property change.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZABLE);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // Test if enabling/disabling |FULLY_LOCKED| toggles the resize lock state
  // properly.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::FULLY_LOCKED);
  EXPECT_TRUE(IsResizeLockEnabled(arc_window));
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZABLE);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // Test nothing will be called by the property overwrite with the same value.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZABLE);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // Test nothing will be called by the NON-interested property change.
  arc_window->SetProperty(kNonInterestedPropKey, true);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));
}

// Test resize lock will not be enabled right after property change but
// will be after the app id is set to the non-null value.
TEST_F(ArcResizeLockManagerTest, TestPropertyChangeWithDelayedAppId) {
  auto* arc_window = CreateFakeWindow(true);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_LIMITED);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));
  // Should ignore null.
  arc_window->ClearProperty(ash::kAppIDKey);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));
  // Should ignore uninterested property change.
  arc_window->SetProperty(kNonInterestedPropKey, true);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));
  // Should not ignore non-null value.
  arc_window->SetProperty(ash::kAppIDKey, new std::string("app-id"));
  EXPECT_TRUE(IsResizeLockEnabled(arc_window));
}

// Tests that resize lock will not be enabled if the resize lock type is changed
// to RESIZABLE while we're waiting for the valid app id.
TEST_F(ArcResizeLockManagerTest, TestPropertyChangeWithDelayedAppIdCancel) {
  auto* arc_window = CreateFakeWindow(true);
  std::string app_id = "app-id";

  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_LIMITED);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZABLE);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  arc_window->SetProperty(ash::kAppIDKey, &app_id);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));
}

// Test that resize lock will NOT be enabled for non ARC windows.
TEST_F(ArcResizeLockManagerTest, TestNonArcWindow) {
  auto* non_arc_window = CreateFakeWindow(false);
  EXPECT_FALSE(IsResizeLockEnabled(non_arc_window));
  non_arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                              ash::ArcResizeLockType::RESIZE_LIMITED);
  EXPECT_FALSE(IsResizeLockEnabled(non_arc_window));
  non_arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                              ash::ArcResizeLockType::RESIZABLE);
  EXPECT_FALSE(IsResizeLockEnabled(non_arc_window));
}

// Test that the ArcResizeLockState is properly handled for the "first-time
// launch" app (whose state is ArcResizeLockState::READY).
TEST_F(ArcResizeLockManagerTest, ResizeLockStateForFirstTimeLaunch) {
  auto* arc_window = CreateFakeWindow(true);
  std::string app_id = "app-id";
  arc_window->SetProperty(ash::kAppIDKey, &app_id);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // Test for RESIZE_LIMITED.
  pref_delegate()->SetResizeLockState(app_id, mojom::ArcResizeLockState::READY);
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_LIMITED);
  EXPECT_EQ(pref_delegate()->GetResizeLockState(app_id),
            mojom::ArcResizeLockState::ON);

  // Test for RESIZABLE.
  pref_delegate()->SetResizeLockState(app_id, mojom::ArcResizeLockState::READY);
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZABLE);
  EXPECT_EQ(pref_delegate()->GetResizeLockState(app_id),
            mojom::ArcResizeLockState::READY);

  // Test for FULLY_LOCKED.
  pref_delegate()->SetResizeLockState(app_id, mojom::ArcResizeLockState::READY);
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::FULLY_LOCKED);
  EXPECT_EQ(pref_delegate()->GetResizeLockState(app_id),
            mojom::ArcResizeLockState::FULLY_LOCKED);
}

// Tests that metrics for initial resize lock state is recorded correctly.
TEST_F(ArcResizeLockManagerTest, TestMetricsForInitialResizeLockState) {
  std::string app_id_resize_locked = "resize-locked-app-id";
  std::string app_id_non_resize_locked = "non-resize-locked-app-id";
  const auto* initial_state_histogram =
      GetResizeLockStateHistogramNameForTesting(
          ResizeLockStateHistogramType::InitialState);
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount(initial_state_histogram, 0);

  // Not record histogram without the app id ready.
  auto* resize_locked_window = CreateFakeWindow(true);
  auto* non_resize_locked_window = CreateFakeWindow(true);
  pref_delegate()->SetResizeLockState(app_id_resize_locked,
                                      mojom::ArcResizeLockState::ON);
  histogram_tester.ExpectTotalCount(initial_state_histogram, 0);

  // Record histogram when the app id is ready.
  resize_locked_window->SetProperty(ash::kAppIDKey, &app_id_resize_locked);
  histogram_tester.ExpectTotalCount(initial_state_histogram, 1);
  histogram_tester.ExpectBucketCount(initial_state_histogram,
                                     mojom::ArcResizeLockState::ON, 1);
  non_resize_locked_window->SetProperty(ash::kAppIDKey,
                                        &app_id_non_resize_locked);
  histogram_tester.ExpectTotalCount(initial_state_histogram, 2);
  histogram_tester.ExpectBucketCount(initial_state_histogram,
                                     mojom::ArcResizeLockState::UNDEFINED, 1);

  // Record histogram only once on initialized.
  pref_delegate()->SetResizeLockState(app_id_resize_locked,
                                      mojom::ArcResizeLockState::OFF);
  histogram_tester.ExpectTotalCount(initial_state_histogram, 2);
}

// Tests that resize shadow type is properly updated according to the resize
// lock type.
TEST_F(ArcResizeLockManagerTest, TestShadowPropertyChange) {
  auto* arc_window = CreateFakeWindow(true);
  arc_window->SetProperty(ash::kAppIDKey, new std::string("app-id"));

  bool resize_shadow_updated = false;
  ScopedWindowPropertyObserver observer(
      arc_window, base::BindLambdaForTesting(
                      [&resize_shadow_updated](aura::Window* window,
                                               const void* key, intptr_t old) {
                        if (key != ash::kResizeShadowTypeKey)
                          return;
                        resize_shadow_updated = true;
                      }));

  // Unlocked by default.
  EXPECT_EQ(arc_window->GetProperty(ash::kResizeShadowTypeKey),
            ash::ResizeShadowType::kUnlock);

  // Locked for resize locked windows.
  resize_shadow_updated = false;
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_LIMITED);
  EXPECT_EQ(arc_window->GetProperty(ash::kResizeShadowTypeKey),
            ash::ResizeShadowType::kLock);
  EXPECT_TRUE(resize_shadow_updated);
  // No redundant property update.
  resize_shadow_updated = false;
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_LIMITED);
  EXPECT_FALSE(resize_shadow_updated);
  resize_shadow_updated = false;
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::FULLY_LOCKED);
  EXPECT_FALSE(resize_shadow_updated);

  // Unlocked for non-resize locked windows.
  resize_shadow_updated = false;
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZABLE);
  EXPECT_EQ(arc_window->GetProperty(ash::kResizeShadowTypeKey),
            ash::ResizeShadowType::kUnlock);
  EXPECT_TRUE(resize_shadow_updated);
  // No redundant property update.
  resize_shadow_updated = false;
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZABLE);
  EXPECT_FALSE(resize_shadow_updated);
}

}  // namespace arc
