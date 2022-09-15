// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"

#include <algorithm>

#include "base/no_destructor.h"

namespace screen_ai {

// static
ScreenAIInstallState* ScreenAIInstallState::GetInstance() {
  static base::NoDestructor<ScreenAIInstallState> g_instance;
  return g_instance.get();
}

ScreenAIInstallState::ScreenAIInstallState() = default;
ScreenAIInstallState::~ScreenAIInstallState() = default;

void ScreenAIInstallState::AddObserver(
    ScreenAIInstallState::Observer* observer) {
  observers_.push_back(observer);
  if (component_ready_)
    observer->ComponentReady();
}

void ScreenAIInstallState::RemoveObserver(
    ScreenAIInstallState::Observer* observer) {
  auto pos = std::find(observers_.begin(), observers_.end(), observer);
  if (pos != observers_.end())
    observers_.erase(pos);
}

void ScreenAIInstallState::SetComponentReady() {
  component_ready_ = true;

  for (ScreenAIInstallState::Observer* observer : observers_)
    observer->ComponentReady();
}

}  // namespace screen_ai
