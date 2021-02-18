// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/test_support/libassistant_service_tester.h"
#include "base/base_paths.h"
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
    : home_dir_override_(base::DIR_HOME),
      service_(service_remote_.BindNewPipeAndPassReceiver(),
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
  mojo::PendingRemote<mojom::AudioOutputDelegate>
      pending_audio_output_delegate_remote;
  mojo::PendingRemote<mojom::MediaDelegate> pending_media_delegate_remote;
  mojo::PendingRemote<mojom::PlatformDelegate> pending_platform_delegate_remote;

  pending_audio_output_delegate_ =
      pending_audio_output_delegate_remote.InitWithNewPipeAndPassReceiver();
  pending_media_delegate_ =
      pending_media_delegate_remote.InitWithNewPipeAndPassReceiver();
  pending_platform_delegate_ =
      pending_platform_delegate_remote.InitWithNewPipeAndPassReceiver();

  service_.Bind(audio_input_controller_.BindNewPipeAndPassReceiver(),
                conversation_controller_.BindNewPipeAndPassReceiver(),
                display_controller_.BindNewPipeAndPassReceiver(),
                media_controller_.BindNewPipeAndPassReceiver(),
                service_controller_.BindNewPipeAndPassReceiver(),
                speaker_id_enrollment_controller_.BindNewPipeAndPassReceiver(),
                std::move(pending_audio_output_delegate_remote),
                std::move(pending_media_delegate_remote),
                std::move(pending_platform_delegate_remote));
}

void LibassistantServiceTester::FlushForTesting() {
  audio_input_controller_.FlushForTesting();
  conversation_controller_.FlushForTesting();
  display_controller_.FlushForTesting();
  media_controller_.FlushForTesting();
  service_controller_.FlushForTesting();
  speaker_id_enrollment_controller_.FlushForTesting();
  service_remote_.FlushForTesting();
}

}  // namespace libassistant
}  // namespace chromeos
