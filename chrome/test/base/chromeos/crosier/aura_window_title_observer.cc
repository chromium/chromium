// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/aura_window_title_observer.h"

#include <map>
#include <string>
#include <tuple>
#include <utility>

#include "ui/aura/test/find_window.h"
#include "ui/aura/window.h"

AuraWindowTitleObserver::AuraWindowTitleObserver(aura::Env* env,
                                                 std::u16string expected_title)
    : ObservationStateObserver(env),
      env_(env),
      expected_title_(std::move(expected_title)) {}

AuraWindowTitleObserver::~AuraWindowTitleObserver() = default;

bool AuraWindowTitleObserver::GetStateObserverInitialState() const {
  // Compute the initial state by checking for existing windows.
  return aura::test::FindWindowWithTitle(env_, expected_title_);
}

void AuraWindowTitleObserver::OnWindowInitialized(aura::Window* window) {
  if (found_) {
    return;
  }

  if (window->GetTitle() == expected_title_) {
    found_ = true;
    windows_.clear();
    OnStateObserverStateChanged(true);
  } else {
    auto [iter, inserted] = windows_.emplace(std::piecewise_construct,
                                             std::forward_as_tuple(window),
                                             std::forward_as_tuple(this));
    CHECK(inserted);
    iter->second.Observe(window);
  }
}

void AuraWindowTitleObserver::OnWillDestroyEnv() {
  env_ = nullptr;
  OnObservationStateObserverSourceDestroyed();
}

void AuraWindowTitleObserver::OnWindowDestroyed(aura::Window* window) {
  windows_.erase(window);
}

void AuraWindowTitleObserver::OnWindowTitleChanged(aura::Window* window) {
  if (window->GetTitle() == expected_title_) {
    found_ = true;
    windows_.clear();
    OnStateObserverStateChanged(true);
  }
}
