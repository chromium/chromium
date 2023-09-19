// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ORCA_ORCA_LIBRARY_H_
#define CHROMEOS_ASH_SERVICES_ORCA_ORCA_LIBRARY_H_

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "base/types/expected.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

struct OrcaLogger;

namespace ash::orca {

// Represents a shared library hosting the Orca service code.
class OrcaLibrary {
 public:
  // Function that receives logs from the shared library.
  using LogSink =
      base::RepeatingCallback<void(logging::LogSeverity, std::string_view)>;

  enum class BindErrorCode {
    kOther = 0,
    kLoadFailed,
    kGetFunctionPointerFailed,
    kBindFailed,
  };

  struct BindError {
    BindErrorCode code;
    std::string message;
  };

  OrcaLibrary();
  explicit OrcaLibrary(const base::FilePath& library_path);
  explicit OrcaLibrary(const base::FilePath& library_path, LogSink log_sink);
  ~OrcaLibrary();
  OrcaLibrary(const OrcaLibrary&) = delete;
  OrcaLibrary& operator=(const OrcaLibrary&) = delete;

  // Forwards `receiver` to the shared library to be bound.
  base::expected<void, BindError> BindReceiver(
      mojo::PendingReceiver<mojom::OrcaService> receiver);

 private:
  base::FilePath library_path_;
  LogSink log_sink_;
  std::unique_ptr<OrcaLogger> orca_logger_;
  base::ScopedNativeLibrary library_;
};

}  // namespace ash::orca

#endif  // CHROMEOS_ASH_SERVICES_ORCA_ORCA_LIBRARY_H_
