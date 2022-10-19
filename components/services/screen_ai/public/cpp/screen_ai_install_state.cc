// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"

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
  if (!component_binary_path_.empty())
    observer->ComponentReady();
}

void ScreenAIInstallState::RemoveObserver(
    ScreenAIInstallState::Observer* observer) {
  auto pos = base::ranges::find(observers_, observer);
  if (pos != observers_.end())
    observers_.erase(pos);
}

void ScreenAIInstallState::SetComponentReady(
    const base::FilePath& component_binary_path) {
  component_binary_path_ = component_binary_path;

  for (ScreenAIInstallState::Observer* observer : observers_)
    observer->ComponentReady();
}

bool ScreenAIInstallState::is_component_ready() {
  return !component_binary_path_.empty();
}

}  // namespace screen_ai
