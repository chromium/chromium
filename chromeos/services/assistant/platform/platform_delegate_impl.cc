// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/platform_delegate_impl.h"
#include "chromeos/services/assistant/public/cpp/assistant_client.h"

namespace chromeos {
namespace assistant {

PlatformDelegateImpl::~PlatformDelegateImpl() = default;
PlatformDelegateImpl::PlatformDelegateImpl() = default;

void PlatformDelegateImpl::Bind(
    mojo::PendingReceiver<PlatformDelegate> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void PlatformDelegateImpl::BindAudioStreamFactory(
    mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) {
  AssistantClient::Get()->RequestAudioStreamFactory(std::move(receiver));
}

void PlatformDelegateImpl::BindAudioDecoderFactory(
    mojo::PendingReceiver<
        ::chromeos::assistant::mojom::AssistantAudioDecoderFactory> receiver) {
  AssistantClient::Get()->RequestAudioDecoderFactory(std::move(receiver));
}

}  // namespace assistant
}  // namespace chromeos
