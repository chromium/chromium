// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/test_support/scoped_assistant_browser_delegate.h"

#include "ash/public/cpp/new_window_delegate.h"

namespace chromeos {
namespace assistant {

namespace {

}  // namespace

ScopedAssistantBrowserDelegate::ScopedAssistantBrowserDelegate() = default;

ScopedAssistantBrowserDelegate::~ScopedAssistantBrowserDelegate() = default;

AssistantBrowserDelegate& ScopedAssistantBrowserDelegate::Get() {
  return *AssistantBrowserDelegate::Get();
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
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction);
}

}  // namespace assistant
}  // namespace chromeos
