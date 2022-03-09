// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/rounded_window_corners_manager.h"

#include <memory>
#include <string>
#include <unordered_set>

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "components/exo/surface.h"
#include "ui/aura/env.h"

namespace chromecast {
namespace {

aura::Window* FindTopmostVisibleNonCornersWindow(
    aura::Window* window,
    const std::unordered_set<aura::Window*>& observed_container_windows) {
  aura::Window* found_window = nullptr;

  if (!window->IsVisible())
    return nullptr;

  if (base::Contains(observed_container_windows, window))
    return window;

  for (auto* child : base::Reversed(window->children())) {
    found_window =
        FindTopmostVisibleNonCornersWindow(child, observed_container_windows);
    if (found_window)
      return found_window;
  }

  return window->GetId() != CastWindowManager::CORNERS_OVERLAY ? window
                                                               : nullptr;
}

bool HasNonAppParent(const aura::Window* window) {
  const aura::Window* parent = window->parent();
  while (parent && parent->IsVisible()) {
    if (parent->GetId() != CastWindowManager::APP)
      return true;
    else
      parent = parent->parent();
  }
  return false;
}

}  // namespace

// Keeps track of the creation and destruction of webview container windows, and
// adds and removes the root window rounded corner decoration accordingly.
// Rounded corners only need to be present when webviews are being displayed.
class RoundedCornersObserver : public aura::WindowObserver,
                               public CastWindowManager::Observer {
 public:
  explicit RoundedCornersObserver(CastWindowManager* cast_window_manager)
      : cast_window_manager_(cast_window_manager) {
    DCHECK(cast_window_manager);
    DCHECK(cast_window_manager_->GetRootWindow());
    cast_window_manager_->AddObserver(this);
    cast_window_manager_->GetRootWindow()->AddObserver(this);
    DecideCorners();
  }

  RoundedCornersObserver(const RoundedCornersObserver&) = delete;
  RoundedCornersObserver& operator=(const RoundedCornersObserver&) = delete;

  ~RoundedCornersObserver() override {
    cast_window_manager_->RemoveObserver(this);
    cast_window_manager_->GetRootWindow()->RemoveObserver(this);
  }

  void OnNewWebviewContainerWindow(aura::Window* window, int app_id) {
    // Observe the lifecycle of this window so we can add rounded corners
    // when it is visible.
    window->AddObserver(this);
    observed_container_windows_.insert(window);
    if (window->IsVisible())
      DecideCorners();
  }

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    DecideCorners();
  }

  void OnWindowDestroyed(aura::Window* window) override {
    if (!base::Contains(observed_container_windows_, window))
      return;

    observed_container_windows_.erase(window);
    DecideCorners();
  }

  void OnWindowHierarchyChanging(const HierarchyChangeParams& params) override {
    DecideCorners();
  }

  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override {
    DecideCorners();
  }

  // CastWindowManager::Observer:
  void WindowOrderChanged() override { DecideCorners(); }

 private:
  void DecideCorners() {
    aura::Window* root_window = cast_window_manager_->GetRootWindow();
    aura::Window* topmost_visible_window = FindTopmostVisibleNonCornersWindow(
        root_window, observed_container_windows_);

    if (!topmost_visible_window)
      return;

    int window_id = topmost_visible_window->GetId();
    // The window may be a child to a visible non-app window that does not draw
    // its own corners, so this needs to be checked for.
    bool set_rounded_corners =
        (window_id != CastWindowManager::APP) ||
        base::Contains(observed_container_windows_, topmost_visible_window) ||
        HasNonAppParent(topmost_visible_window);

    if (rounded_corners_ == set_rounded_corners)
      return;

    cast_window_manager_->SetEnableRoundedCorners(set_rounded_corners);
    rounded_corners_ = set_rounded_corners;
  }

  bool rounded_corners_ = false;
  CastWindowManager* cast_window_manager_;
  std::unordered_set<aura::Window*> observed_container_windows_;
};

RoundedWindowCornersManager::RoundedWindowCornersManager(
    CastWindowManager* cast_window_manager)
    : rounded_corners_observer_(
          std::make_unique<RoundedCornersObserver>(cast_window_manager)) {
  aura::Env::GetInstance()->AddObserver(this);
}

RoundedWindowCornersManager::~RoundedWindowCornersManager() {
  aura::Env::GetInstance()->RemoveObserver(this);
}

void RoundedWindowCornersManager::OnWindowInitialized(aura::Window* window) {
  window->AddObserver(this);
}

void RoundedWindowCornersManager::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
}

void RoundedWindowCornersManager::OnWindowPropertyChanged(aura::Window* window,
                                                          const void* key,
                                                          intptr_t old) {
  if (key != exo::kClientSurfaceIdKey)
    return;

  // Note: The property was originally an integer, and was switched to be a
  // string. For compatibility integer values are converted to a string via
  // base::NumberToString before being set as the property value.
  std::string* app_id_str = window->GetProperty(exo::kClientSurfaceIdKey);
  if (!app_id_str)
    return;

  int app_id = 0;
  if (!base::StringToInt(*app_id_str, &app_id))
    return;

  LOG(INFO) << "Found window for webview " << app_id;
  rounded_corners_observer_->OnNewWebviewContainerWindow(window, app_id);
}

}  // namespace chromecast
