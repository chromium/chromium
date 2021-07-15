// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_DECODER_SYSTEM_ENGINE_H_
#define CHROMEOS_SERVICES_IME_DECODER_SYSTEM_ENGINE_H_

#include "base/scoped_native_library.h"
#include "chromeos/services/ime/ime_decoder.h"
#include "chromeos/services/ime/input_engine.h"
#include "chromeos/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/services/ime/public/mojom/input_method_host.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace ime {

// An enhanced implementation of the basic InputEngine that uses a built-in
// shared library for handling key events.
class SystemEngine : public InputEngine {
 public:
  explicit SystemEngine(ImeCrosPlatform* platform);
  SystemEngine(const SystemEngine&) = delete;
  SystemEngine& operator=(const SystemEngine&) = delete;
  ~SystemEngine() override;

  // Binds the mojom::InputMethod interface to this object and returns true if
  // the given ime_spec is supported by the engine.
  bool BindRequest(const std::string& ime_spec,
                   mojo::PendingReceiver<mojom::InputMethod> receiver,
                   mojo::PendingRemote<mojom::InputMethodHost> host);

  // InputEngine:
  bool IsConnected() override;

 private:
  // Try to load the decoding functions from some decoder shared library.
  // Returns whether loading decoder is successful.
  bool TryLoadDecoder();

  // Returns whether the decoder shared library supports this ime_spec.
  bool IsImeSupportedByDecoder(const std::string& ime_spec);

  ImeCrosPlatform* platform_ = nullptr;

  absl::optional<ImeDecoder::EntryPoints> decoder_entry_points_;
};

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_DECODER_SYSTEM_ENGINE_H_
