// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/test_support/libassistant_service_tester.h"
#include "services/network/test/test_url_loader_factory.h"

namespace chromeos {
namespace libassistant {

namespace {

mojo::PendingRemote<network::mojom::URLLoaderFactory> BindURLLoaderFactory() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> result;
  network::TestURLLoaderFactory().Clone(
      result.InitWithNewPipeAndPassReceiver());
  return result;
}

}  // namespace

LibassistantServiceTester::LibassistantServiceTester()
    : service_(service_remote_.BindNewPipeAndPassReceiver(),
               /*platform_api=*/nullptr,
               &assistant_manager_service_delegate_) {
  BindControllers();
}

LibassistantServiceTester::~LibassistantServiceTester() = default;

void LibassistantServiceTester::Start() {
  service_controller_->Initialize(mojom::BootupConfig::New(),
                                  BindURLLoaderFactory());
  service_controller_->Start();
  service_controller_.FlushForTesting();
}

void LibassistantServiceTester::BindControllers() {
  mojo::PendingRemote<mojom::AudioStreamFactoryDelegate>
      pending_audio_stream_factory_delegate_remote;
  pending_audio_stream_factory_delegate_ =
      pending_audio_stream_factory_delegate_remote
          .InitWithNewPipeAndPassReceiver();

  service_.Bind(audio_input_controller_.BindNewPipeAndPassReceiver(),
                std::move(pending_audio_stream_factory_delegate_remote),
                conversation_controller_.BindNewPipeAndPassReceiver(),
                display_controller_.BindNewPipeAndPassReceiver(),
                service_controller_.BindNewPipeAndPassReceiver());
}

}  // namespace libassistant
}  // namespace chromeos
