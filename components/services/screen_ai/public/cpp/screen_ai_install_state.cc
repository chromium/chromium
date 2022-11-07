// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "components/services/screen_ai/public/cpp/utilities.h"

namespace screen_ai {

// static
ScreenAIInstallState* ScreenAIInstallState::GetInstance() {
  static base::NoDestructor<ScreenAIInstallState> instance;
  return instance.get();
}

ScreenAIInstallState::ScreenAIInstallState() = default;
ScreenAIInstallState::~ScreenAIInstallState() = default;

void ScreenAIInstallState::AddObserver(
    ScreenAIInstallState::Observer* observer) {
  observers_.push_back(observer);
  if (is_component_ready())
    observer->ComponentReady();
}

void ScreenAIInstallState::RemoveObserver(
    ScreenAIInstallState::Observer* observer) {
  auto pos = base::ranges::find(observers_, observer);
  if (pos != observers_.end())
    observers_.erase(pos);
}

void ScreenAIInstallState::SetComponentReady(
    const base::FilePath& component_folder) {
  component_binary_path_ =
      component_folder.Append(GetComponentBinaryFileName());
  component_ready_ = true;

  for (ScreenAIInstallState::Observer* observer : observers_)
    observer->ComponentReady();
}

}  // namespace screen_ai
