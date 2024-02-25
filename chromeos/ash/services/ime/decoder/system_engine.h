// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_DECODER_SYSTEM_ENGINE_H_
#define CHROMEOS_ASH_SERVICES_IME_DECODER_SYSTEM_ENGINE_H_

#include <optional>

#include "base/scoped_native_library.h"
#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/ash/services/ime/public/mojom/connection_factory.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_host.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace ime {

// An enhanced implementation of the basic InputEngine that uses a built-in
// shared library for handling key events.
// TODO(b/214153032): Rename to MojoModeSharedLibEngine, and maybe also unnest
// out of "decoder" sub-directory, to better reflect what this represents. This
// class actually wraps MojoMode "C" API entry points of the loaded CrOS 1P IME
// shared lib, to facilitate accessing an IME engine therein via MojoMode.
class SystemEngine {
 public:
  explicit SystemEngine(
      ImeCrosPlatform* platform,
      std::optional<ImeSharedLibraryWrapper::EntryPoints> entry_points);

  SystemEngine(const SystemEngine&) = delete;
  SystemEngine& operator=(const SystemEngine&) = delete;
  ~SystemEngine();

  // Binds the mojom::ConnectionFactory interface in the shared library.
  bool BindConnectionFactory(
      mojo::PendingReceiver<mojom::ConnectionFactory> receiver);

  bool IsConnected();

 private:
  std::optional<ImeSharedLibraryWrapper::EntryPoints> decoder_entry_points_;
};

}  // namespace ime
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_IME_DECODER_SYSTEM_ENGINE_H_
