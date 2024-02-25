// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_DECODER_DECODER_ENGINE_H_
#define CHROMEOS_ASH_SERVICES_IME_DECODER_DECODER_ENGINE_H_

#include <optional>

#include "base/scoped_native_library.h"
#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/ash/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_host.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace ime {

// A Mojo wrapper around a "decoder" that converts key events and pointer events
// to text. The built-in Chrome OS XKB extension communicates with this to
// implement its IMEs.
// TODO(b/214153032): Rename to ProtoModeSharedLibEngine, and maybe also unnest
// out of "decoder" sub-directory, to better reflect what this represents. This
// class actually wraps ProtoMode "C" API entry points of the loaded CrOS 1P IME
// shared lib, to facilitate accessing an IME engine therein via ProtoMode.
class DecoderEngine : public mojom::InputChannel {
 public:
  explicit DecoderEngine(
      ImeCrosPlatform* platform,
      std::optional<ImeSharedLibraryWrapper::EntryPoints> entry_points);

  DecoderEngine(const DecoderEngine&) = delete;
  DecoderEngine& operator=(const DecoderEngine&) = delete;

  ~DecoderEngine() override;

  // Binds the mojom::InputChannel interface to this object and returns true if
  // the given ime_spec is supported by the engine.
  bool BindRequest(const std::string& ime_spec,
                   mojo::PendingReceiver<mojom::InputChannel> receiver,
                   mojo::PendingRemote<mojom::InputChannel> remote,
                   const std::vector<uint8_t>& extra);

  // mojom::InputChannel:
  void ProcessMessage(const std::vector<uint8_t>& message,
                      ProcessMessageCallback callback) override;

 private:
  std::optional<ImeSharedLibraryWrapper::EntryPoints> decoder_entry_points_;
  mojo::ReceiverSet<mojom::InputChannel> decoder_channel_receivers_;
};

}  // namespace ime
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_IME_DECODER_DECODER_ENGINE_H_
