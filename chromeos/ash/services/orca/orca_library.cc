// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/orca/orca_library.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "base/types/expected.h"
#include "chromeos/ash/services/orca/public/cpp/orca_entry.h"
#include "mojo/public/c/system/thunks.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::orca {
namespace {

base::FilePath GetDefaultOrcaLibraryPath() {
  // A relative path is fine because base::ScopedNativeLibrary will look for the
  // library in an architecture-dependent system path.
  return base::FilePath("libimedecoder.so");
}

logging::LogSeverity ConvertLogSeverity(OrcaLogSeverity severity) {
  switch (severity) {
    case OrcaLogSeverity::ORCA_LOG_SEVERITY_WARNING:
      return logging::LOGGING_WARNING;
    case OrcaLogSeverity::ORCA_LOG_SEVERITY_ERROR:
      return logging::LOGGING_ERROR;
  }
  NOTREACHED();
}

void DefaultLogSink(logging::LogSeverity severity, std::string_view message) {
  switch (severity) {
    case logging::LOGGING_WARNING:
      LOG(WARNING) << message;
      break;
    case logging::LOGGING_ERROR:
      LOG(ERROR) << message;
      break;
    default:
      NOTREACHED();
  }
}

std::unique_ptr<OrcaLogger> CreateOrcaLogger(OrcaLibrary::LogSink* log_sink) {
  auto logger = std::make_unique<OrcaLogger>();
  logger->user_data = reinterpret_cast<void*>(log_sink),
  logger->log = [](OrcaLogger* self, OrcaLogSeverity severity,
                   const char* message) {
    reinterpret_cast<OrcaLibrary::LogSink*>(self->user_data)
        ->Run(ConvertLogSeverity(severity), message);
  };
  return logger;
}

}  // namespace

OrcaLibrary::OrcaLibrary() : OrcaLibrary(GetDefaultOrcaLibraryPath()) {}

OrcaLibrary::OrcaLibrary(const base::FilePath& library_path)
    : OrcaLibrary(library_path, base::BindRepeating(DefaultLogSink)) {}

OrcaLibrary::OrcaLibrary(const base::FilePath& library_path, LogSink log_sink)
    : library_path_(library_path),
      log_sink_(std::move(log_sink)),
      orca_logger_(CreateOrcaLogger(&log_sink_)) {}

OrcaLibrary::~OrcaLibrary() {
  if (!library_.is_valid()) {
    return;
  }

  if (auto* reset_function = reinterpret_cast<decltype(OrcaResetService)*>(
          library_.GetFunctionPointer("OrcaResetService"))) {
    reset_function();
  }
}

base::expected<void, OrcaLibrary::BindError> OrcaLibrary::BindReceiver(
    mojo::PendingReceiver<mojom::OrcaService> receiver) {
  library_ = base::ScopedNativeLibrary(library_path_);
  if (!library_.is_valid()) {
    return base::unexpected(BindError{
        .code = BindErrorCode::kLoadFailed,
        .message = library_.GetError()->ToString(),
    });
  }

  auto* bind_function = reinterpret_cast<decltype(OrcaBindService)*>(
      library_.GetFunctionPointer("OrcaBindService"));
  if (!bind_function) {
    return base::unexpected(
        BindError{.code = BindErrorCode::kGetFunctionPointerFailed});
  }

  const MojoSystemThunks2* mojo_thunks = MojoEmbedderGetSystemThunks2();
  const MojoSystemThunks* mojo_thunks_legacy = MojoEmbedderGetSystemThunks32();
  const MojoHandle receiver_handle = receiver.PassPipe().release().value();

  if (OrcaBindServiceStatus code = bind_function(
          mojo_thunks, mojo_thunks_legacy, receiver_handle, orca_logger_.get());
      code != OrcaBindServiceStatus::ORCA_BIND_SERVICE_STATUS_OK) {
    return base::unexpected(BindError{.code = BindErrorCode::kBindFailed});
  }

  return base::ok();
}

}  // namespace ash::orca
