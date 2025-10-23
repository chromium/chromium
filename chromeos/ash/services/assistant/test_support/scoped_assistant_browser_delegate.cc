// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/test_support/scoped_assistant_browser_delegate.h"

#include <optional>
#include <string>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "base/types/expected.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"

namespace ash::assistant {

ScopedAssistantBrowserDelegate::ScopedAssistantBrowserDelegate() = default;

ScopedAssistantBrowserDelegate::~ScopedAssistantBrowserDelegate() = default;

AssistantBrowserDelegate& ScopedAssistantBrowserDelegate::Get() {
  return *AssistantBrowserDelegate::Get();
}

void ScopedAssistantBrowserDelegate::SetOpenNewEntryPointClosure(
    base::OnceClosure closure) {
  CHECK(open_new_entry_point_closure_.is_null()) << "Closure is already set";
  open_new_entry_point_closure_ = std::move(closure);
}

void ScopedAssistantBrowserDelegate::OpenUrl(GURL url) {
  NewWindowDelegate::GetInstance()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

base::expected<bool, AssistantBrowserDelegate::Error>
ScopedAssistantBrowserDelegate::IsNewEntryPointEligibleForPrimaryProfile() {
  return true;
}

void ScopedAssistantBrowserDelegate::OpenNewEntryPoint() {
  if (open_new_entry_point_closure_.is_null()) {
    return;
  }

  std::move(open_new_entry_point_closure_).Run();
}

std::optional<std::string>
ScopedAssistantBrowserDelegate::GetNewEntryPointName() {
  return "New entry point";
}

}  // namespace ash::assistant
