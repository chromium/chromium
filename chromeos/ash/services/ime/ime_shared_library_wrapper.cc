// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chromeos/ash/services/ime/constants.h"

namespace ash {
namespace ime {

namespace {

const char kCrosImeDecoderLib[] = "libimedecoder.so";

// TODO(b/161491092): Add test image path based on value of
// "CHROMEOS_RELEASE_TRACK" from `base::SysInfo::GetLsbReleaseValue`.
// Returns ImeDecoderLib path based on the run time env.
base::FilePath GetImeDecoderLibPath() {
#if defined(__x86_64__) || defined(__aarch64__)
  base::FilePath lib_path("/usr/lib64");
#else
  base::FilePath lib_path("/usr/lib");
#endif
  lib_path = lib_path.Append(kCrosImeDecoderLib);
  return lib_path;
}

// Simple bridge between logging in the loaded shared library and logging in
// Chrome.
void ImeLoggerBridge(int severity, const char* message) {
  switch (severity) {
    case logging::LOG_INFO:
      // TODO(b/162375823): VLOG_IF(INFO, is_debug_version).
      break;
    case logging::LOG_WARNING:
      LOG(WARNING) << message;
      break;
    case logging::LOG_ERROR:
      LOG(ERROR) << message;
      break;
    case logging::LOG_FATAL:
      LOG(FATAL) << message;
      break;
    default:
      break;
  }
}

}  // namespace

ImeSharedLibraryWrapperImpl::ImeSharedLibraryWrapperImpl() = default;

absl::optional<ImeSharedLibraryWrapper::EntryPoints>
ImeSharedLibraryWrapperImpl::MaybeLoadThenReturnEntryPoints() {
  if (entry_points_) {
    return entry_points_;
  }

  base::FilePath path = GetImeDecoderLibPath();

  // Add dlopen flags (RTLD_LAZY | RTLD_NODELETE) later.
  base::ScopedNativeLibrary library = base::ScopedNativeLibrary(path);
  if (!library.is_valid()) {
    LOG(ERROR) << "Failed to load decoder shared library from: " << path
               << ", error: " << library.GetError()->ToString();
    return absl::nullopt;
  }

  EntryPoints entry_points = {
      .init_proto_mode = reinterpret_cast<InitProtoModeFn>(
          library.GetFunctionPointer(kInitProtoModeFnName)),
      .close_proto_mode = reinterpret_cast<CloseProtoModeFn>(
          library.GetFunctionPointer(kCloseProtoModeFnName)),
      .supports = reinterpret_cast<ImeDecoderSupportsFn>(
          library.GetFunctionPointer(kImeDecoderSupportsFnName)),
      .activate_ime = reinterpret_cast<ImeDecoderActivateImeFn>(
          library.GetFunctionPointer(kImeDecoderActivateImeFnName)),
      .process = reinterpret_cast<ImeDecoderProcessFn>(
          library.GetFunctionPointer(kImeDecoderProcessFnName)),
      .init_mojo_mode = reinterpret_cast<InitMojoModeFn>(
          library.GetFunctionPointer(kInitMojoModeFnName)),
      .close_mojo_mode = reinterpret_cast<CloseMojoModeFn>(
          library.GetFunctionPointer(kCloseMojoModeFnName)),
      .initialize_connection_factory =
          reinterpret_cast<InitializeConnectionFactoryFn>(
              library.GetFunctionPointer(kInitializeConnectionFactoryFnName)),
      .is_input_method_connected = reinterpret_cast<IsInputMethodConnectedFn>(
          library.GetFunctionPointer(kIsInputMethodConnectedFnName)),
  };

  // Checking if entry_points are loaded.
  if (!entry_points.init_proto_mode || !entry_points.close_proto_mode ||
      !entry_points.supports || !entry_points.activate_ime ||
      !entry_points.process || !entry_points.init_mojo_mode ||
      !entry_points.close_mojo_mode ||
      !entry_points.is_input_method_connected ||
      !entry_points.initialize_connection_factory) {
    return absl::nullopt;
  }

  // Optional function pointer.
  SetImeEngineLoggerFn logger_setter = reinterpret_cast<SetImeEngineLoggerFn>(
      library.GetFunctionPointer(kSetImeEngineLoggerFnName));
  if (logger_setter) {
    logger_setter(ImeLoggerBridge);
  } else {
    LOG(WARNING) << "Failed to set a Chrome Logger for decoder DSO.";
  }

  library_ = std::move(library);
  entry_points_ = entry_points;
  return entry_points_;
}

ImeSharedLibraryWrapperImpl::~ImeSharedLibraryWrapperImpl() = default;

ImeSharedLibraryWrapperImpl* ImeSharedLibraryWrapperImpl::GetInstance() {
  static base::NoDestructor<ImeSharedLibraryWrapperImpl> instance;
  return instance.get();
}

}  // namespace ime
}  // namespace ash
