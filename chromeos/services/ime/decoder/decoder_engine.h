// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_DECODER_DECODER_ENGINE_H_
#define CHROMEOS_SERVICES_IME_DECODER_DECODER_ENGINE_H_

#include "base/scoped_native_library.h"
#include "chromeos/services/ime/input_engine.h"
#include "chromeos/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace chromeos {
namespace ime {

// An enhanced implementation of the basic InputEngine which allows the input
// engine to call a customized transliteration library (aka decoder) to provide
// a premium typing experience.
class DecoderEngine : public InputEngine {
 public:
  explicit DecoderEngine(ImeCrosPlatform* platform);
  ~DecoderEngine() override;

  // InputEngine overrides:
  bool BindRequest(const std::string& ime_spec,
                   mojo::PendingReceiver<mojom::InputChannel> receiver,
                   mojo::PendingRemote<mojom::InputChannel> remote,
                   const std::vector<uint8_t>& extra) override;

  void ProcessMessage(const std::vector<uint8_t>& message,
                      ProcessMessageCallback callback) override;

 private:
  // Try to load the decoding functions from some decoder shared library.
  // Returns whether loading decoder is successful.
  bool TryLoadDecoder();

  // Returns whether the decoder shared library supports this ime_spec.
  bool IsImeSupportedByDecoder(const std::string& ime_spec);

  // Shared library handle of the implementation for input logic with decoders.
  base::ScopedNativeLibrary library_;

  ImeEngineMainEntry* engine_main_entry_ = nullptr;

  ImeCrosPlatform* platform_ = nullptr;

  mojo::ReceiverSet<mojom::InputChannel> decoder_channel_receivers_;

  DISALLOW_COPY_AND_ASSIGN(DecoderEngine);
};

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_DECODER_DECODER_ENGINE_H_
