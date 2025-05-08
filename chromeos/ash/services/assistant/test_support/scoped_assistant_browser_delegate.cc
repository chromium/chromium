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
#include "chromeos/ash/services/assistant/public/cpp/features.h"

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

void ScopedAssistantBrowserDelegate::SetMediaControllerManager(
    mojo::Receiver<media_session::mojom::MediaControllerManager>* receiver) {
  media_controller_manager_receiver_ = receiver;
}

void ScopedAssistantBrowserDelegate::RequestMediaControllerManager(
    mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
        receiver) {
  if (media_controller_manager_receiver_) {
    media_controller_manager_receiver_->reset();
    media_controller_manager_receiver_->Bind(std::move(receiver));
  }
}

void ScopedAssistantBrowserDelegate::OpenUrl(GURL url) {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

base::expected<bool, AssistantBrowserDelegate::Error>
ScopedAssistantBrowserDelegate::IsNewEntryPointEligibleForPrimaryProfile() {
  if (!ash::assistant::features::IsNewEntryPointEnabled()) {
    return base::unexpected(
        AssistantBrowserDelegate::Error::kNewEntryPointNotEnabled);
  }

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
