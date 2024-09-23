// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/services/ime/constants.h"

namespace ash {
namespace ime {

namespace {

const char kCrosImeDecoderLib[] = "libimedecoder.so";

base::FilePath GetImeDecoderLibPath() {
#if defined(__x86_64__) || defined(__aarch64__)
  base::FilePath lib_path("/usr/lib64");
#else
  base::FilePath lib_path("/usr/lib");
#endif
  return lib_path.Append(kCrosImeDecoderLib);
}

// Simple bridge between logging in the loaded shared library and logging in
// Chrome.
// Severity comes from the LogSeverity enum in absl logging.
void ImeLoggerBridge(int severity, const char* message) {
  switch (severity) {
    case logging::LOGGING_INFO:
      // Silently ignore.
      break;
    case logging::LOGGING_WARNING:
      LOG(WARNING) << message;
      break;
    case logging::LOGGING_ERROR:
      LOG(ERROR) << message;
      break;
    case logging::LOGGING_FATAL:
      LOG(FATAL) << message;
    default:
      // There's no LOGGING_VERBOSE level in absl logging. Nothing should reach
      // here.
      NOTREACHED();
  }
}

}  // namespace

ImeSharedLibraryWrapperImpl::ImeSharedLibraryWrapperImpl() = default;

std::optional<ImeSharedLibraryWrapper::EntryPoints>
ImeSharedLibraryWrapperImpl::MaybeLoadThenReturnEntryPoints() {
  if (entry_points_) {
    return entry_points_;
  }

  base::FilePath path = GetImeDecoderLibPath();

  // Add dlopen flags (RTLD_LAZY | RTLD_NODELETE) later.
  base::ScopedNativeLibrary library = base::ScopedNativeLibrary(path);
  if (!library.is_valid()) {
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "Failed to load decoder shared library from: " << path
        << ", error: " << library.GetError()->ToString();
    return std::nullopt;
  }

  EntryPoints entry_points = {
      .init_proto_mode = reinterpret_cast<InitProtoModeFn>(
          library.GetFunctionPointer(kInitProtoModeFnName)),
      .close_proto_mode = reinterpret_cast<CloseProtoModeFn>(
          library.GetFunctionPointer(kCloseProtoModeFnName)),
      .proto_mode_supports = reinterpret_cast<ImeDecoderSupportsFn>(
          library.GetFunctionPointer(kImeDecoderSupportsFnName)),
      .proto_mode_activate_ime = reinterpret_cast<ImeDecoderActivateImeFn>(
          library.GetFunctionPointer(kImeDecoderActivateImeFnName)),
      .proto_mode_process = reinterpret_cast<ImeDecoderProcessFn>(
          library.GetFunctionPointer(kImeDecoderProcessFnName)),
      .init_mojo_mode = reinterpret_cast<InitMojoModeFn>(
          library.GetFunctionPointer(kInitMojoModeFnName)),
      .close_mojo_mode = reinterpret_cast<CloseMojoModeFn>(
          library.GetFunctionPointer(kCloseMojoModeFnName)),
      .mojo_mode_initialize_connection_factory =
          reinterpret_cast<InitializeConnectionFactoryFn>(
              library.GetFunctionPointer(kInitializeConnectionFactoryFnName)),
      .mojo_mode_is_input_method_connected =
          reinterpret_cast<IsInputMethodConnectedFn>(
              library.GetFunctionPointer(kIsInputMethodConnectedFnName)),
      .init_user_data_service = reinterpret_cast<InitUserDataServiceFn>(
          library.GetFunctionPointer(kInitUserDataServiceFnName)),
      .process_user_data_request = reinterpret_cast<ProcessUserDataRequestFn>(
          library.GetFunctionPointer(kProcessUserDataRequestFnName)),
      .delete_serialized_proto = reinterpret_cast<DeleteSerializedProtoFn>(
          library.GetFunctionPointer(kDeleteSerializedProtoFnName)),
  };

  // Checking if entry_points are loaded.
  // TODO(b/328997024): Add .init_user_data_service check once implemented in
  // sharedlib.
  if (!entry_points.init_proto_mode || !entry_points.close_proto_mode ||
      !entry_points.proto_mode_supports ||
      !entry_points.proto_mode_activate_ime ||
      !entry_points.proto_mode_process || !entry_points.init_mojo_mode ||
      !entry_points.close_mojo_mode ||
      !entry_points.mojo_mode_is_input_method_connected ||
      !entry_points.mojo_mode_initialize_connection_factory) {
    return std::nullopt;
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
