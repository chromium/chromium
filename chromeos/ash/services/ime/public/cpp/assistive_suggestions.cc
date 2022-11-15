// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"

namespace ash {
namespace ime {

AssistiveWindow::AssistiveWindow() : type(AssistiveWindowType::kNone) {}

AssistiveWindow::AssistiveWindow(
    const AssistiveWindowType& type,
    const std::vector<AssistiveSuggestion>& candidates)
    : type(type), candidates(std::move(candidates)) {}

AssistiveWindow::AssistiveWindow(const AssistiveWindow& window) = default;
AssistiveWindow::~AssistiveWindow() = default;

}  // namespace ime
}  // namespace ash
