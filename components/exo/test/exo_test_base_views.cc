// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_base_views.h"

#include "components/exo/vsync_timing_manager.h"
#include "components/exo/wm_helper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/wm_core_switches.h"

namespace exo {
namespace test {

namespace {

class WMHelperTester : public WMHelper, public VSyncTimingManager::Delegate {
 public:
  WMHelperTester(aura::Window* root_window)
      : root_window_(root_window), vsync_timing_manager_(this) {}
  ~WMHelperTester() override {}

  // Overridden from WMHelper
  void AddActivationObserver(wm::ActivationChangeObserver* observer) override {}
  void RemoveActivationObserver(
      wm::ActivationChangeObserver* observer) override {}
  void AddFocusObserver(aura::client::FocusChangeObserver* observer) override {}
  void RemoveFocusObserver(
      aura::client::FocusChangeObserver* observer) override {}
  void AddDragDropObserver(DragDropObserver* observer) override {}
  void RemoveDragDropObserver(DragDropObserver* observer) override {}
  void SetDragDropDelegate(aura::Window*) override {}
  void ResetDragDropDelegate(aura::Window*) override {}
  VSyncTimingManager& GetVSyncTimingManager() override {
    return vsync_timing_manager_;
  }

  const display::ManagedDisplayInfo& GetDisplayInfo(
      int64_t display_id) const override {
    static display::ManagedDisplayInfo md;
    return md;
  }
  const std::vector<uint8_t>& GetDisplayIdentificationData(
      int64_t display_id) const override {
    static std::vector<uint8_t> no_data;
    return no_data;
  }
  bool GetActiveModeForDisplayId(
      int64_t display_id,
      display::ManagedDisplayMode* mode) const override {
    return false;
  }

  aura::Window* GetPrimaryDisplayContainer(int container_id) override {
    return root_window_;
  }
  aura::Window* GetActiveWindow() const override { return nullptr; }
  aura::Window* GetFocusedWindow() const override { return nullptr; }
  aura::Window* GetRootWindowForNewWindows() const override {
    return root_window_;
  }
  aura::client::CursorClient* GetCursorClient() override { return nullptr; }
  void AddPreTargetHandler(ui::EventHandler* handler) override {}
  void PrependPreTargetHandler(ui::EventHandler* handler) override {}
  void RemovePreTargetHandler(ui::EventHandler* handler) override {}
  void AddPostTargetHandler(ui::EventHandler* handler) override {}
  void RemovePostTargetHandler(ui::EventHandler* handler) override {}
  bool InTabletMode() const override { return false; }
  double GetDefaultDeviceScaleFactor() const override { return 1.0; }
  void SetImeBlocked(aura::Window* window, bool ime_blocked) override {}
  bool IsImeBlocked(aura::Window* window) const override { return false; }

  LifetimeManager* GetLifetimeManager() override { return &lifetime_manager_; }
  aura::client::CaptureClient* GetCaptureClient() override { return nullptr; }

  // Overridden from aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override {}
  int OnDragUpdated(const ui::DropTargetEvent& event) override { return 0; }
  void OnDragExited() override {}
  int OnPerformDrop(const ui::DropTargetEvent& event,
                    std::unique_ptr<ui::OSExchangeData> data) override {
    return 0;
  }

  // Overridden from VSyncTimingManager::Delegate:
  void AddVSyncParameterObserver(
      mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer)
      override {}

 private:
  aura::Window* root_window_;
  LifetimeManager lifetime_manager_;
  VSyncTimingManager vsync_timing_manager_;

  DISALLOW_COPY_AND_ASSIGN(WMHelperTester);
};

}  // namespace

ExoTestBaseViews::ExoTestBaseViews() {}
ExoTestBaseViews::~ExoTestBaseViews() {}

void ExoTestBaseViews::SetUp() {
  views::ViewsTestBase::SetUp();
  // Takes care of its own lifetime.
  new wm::DefaultActivationClient(root_window());

  wm_helper_ = std::make_unique<WMHelperTester>(root_window());
  WMHelper::SetInstance(wm_helper_.get());
}

void ExoTestBaseViews::TearDown() {
  WMHelper::SetInstance(nullptr);
  wm_helper_.reset();

  views::ViewsTestBase::TearDown();
}

}  // namespace test
}  // namespace exo
