// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/window_occlusion_manager.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"

namespace exo {
namespace {

void RecursivelyPauseAllTrackedWindows(
    aura::Window* window,
    WindowOcclusionManager::StateLockers* lockers) {
  if (window->GetOcclusionState() != aura::Window::OcclusionState::UNKNOWN) {
    lockers->push_back(
        std::make_unique<aura::WindowOcclusionTracker::ScopedLockState>(
            window));
  }
  for (auto child : window->children()) {
    RecursivelyPauseAllTrackedWindows(child.get(), lockers);
  }
}

}  // namespace

WindowOcclusionManager::WindowOcclusionManager() {
  WMHelper::GetInstance()->AddExoWindowObserver(this);
  ash::Shell::Get()->session_controller()->AddObserver(this);
}

WindowOcclusionManager::~WindowOcclusionManager() {
  WMHelper::GetInstance()->RemoveExoWindowObserver(this);
  ash::Shell::Get()->session_controller()->RemoveObserver(this);
}

void WindowOcclusionManager::OnSessionStateChanged(
    session_manager::SessionState state) {
  bool locked = ash::Shell::Get()->session_controller()->IsScreenLocked();
  if (!locked) {
    // Just clear the vector for all non locked state as the cost of clearing an
    // empty vector is negligible.
    state_lockers_.clear();
  } else {
    for (aura::Window* window : tracked_window_observations_.sources()) {
      RecursivelyPauseAllTrackedWindows(window, &state_lockers_);
    }
  }
}

void WindowOcclusionManager::OnWindowDestroying(aura::Window* window) {
  tracked_window_observations_.RemoveObservation(window);
}

void WindowOcclusionManager::OnExoWindowCreated(aura::Window* window) {
  tracked_window_observations_.AddObservation(window);
}

}  // namespace exo
