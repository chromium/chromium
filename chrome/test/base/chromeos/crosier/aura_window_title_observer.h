// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_AURA_WINDOW_TITLE_OBSERVER_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_AURA_WINDOW_TITLE_OBSERVER_H_

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/base/interaction/state_observer.h"

namespace aura {
class Env;
class EnvObserver;
}  // namespace aura

// Watches for windows created with a given title.
class AuraWindowTitleObserver
    : public ui::test::
          ObservationStateObserver<bool, aura::Env, aura::EnvObserver>,
      public aura::WindowObserver {
 public:
  AuraWindowTitleObserver(aura::Env* env, std::u16string expected_title);
  ~AuraWindowTitleObserver() override;

  // ui::test::StateObserver:
  bool GetStateObserverInitialState() const override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;
  void OnWillDestroyEnv() override;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowTitleChanged(aura::Window* window) override;

 private:
  raw_ptr<aura::Env> env_;
  const std::u16string expected_title_;
  bool found_ = false;

  using ScopedWindowObservation =
      base::ScopedObservation<aura::Window, aura::WindowObserver>;

  std::map<aura::Window*, ScopedWindowObservation> windows_;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_AURA_WINDOW_TITLE_OBSERVER_H_
