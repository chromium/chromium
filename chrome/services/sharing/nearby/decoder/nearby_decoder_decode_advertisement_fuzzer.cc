// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/decoder/nearby_decoder.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder_types.mojom.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/remote.h"

struct Environment {
  Environment() {
    mojo::core::Init();
    // Disable noisy logging as per "libFuzzer in Chrome" documentation:
    // testing/libfuzzer/getting_started.md#Disable-noisy-error-message-logging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);

    // Create instance once to be reused between fuzzing rounds.
    decoder = std::make_unique<sharing::NearbySharingDecoder>(
        remote.BindNewPipeAndPassReceiver(),
        /*on_disconnect=*/base::DoNothing());
  }

  base::SingleThreadTaskExecutor task_executor;
  mojo::Remote<::sharing::mojom::NearbySharingDecoder> remote;
  std::unique_ptr<sharing::NearbySharingDecoder> decoder;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static base::NoDestructor<Environment> environment;

  std::vector<uint8_t> buffer(data, data + size);
  base::RunLoop run_loop;
  environment->decoder->DecodeAdvertisement(
      buffer, base::BindOnce(
                  [](base::RunLoop* run_loop,
                     ::sharing::mojom::AdvertisementPtr advertisement) {
                    run_loop->Quit();
                  },
                  &run_loop));
  run_loop.Run();

  return 0;
}
