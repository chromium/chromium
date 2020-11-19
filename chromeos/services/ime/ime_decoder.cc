// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_decoder.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "chromeos/services/ime/constants.h"

namespace chromeos {
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
    default:
      break;
  }
}

// Check whether the crucial members of an EntryPoints are loaded.
bool isEntryPointsLoaded(ImeDecoder::EntryPoints entry) {
  return (entry.initOnce && entry.support && entry.activateIme &&
          entry.process && entry.closeDecoder);
}

}  // namespace

ImeDecoder::ImeDecoder() : status_(Status::kUninitialized) {
  base::FilePath path = GetImeDecoderLibPath();

  if (!base::PathExists(path)) {
    LOG(WARNING) << "IME decoder shared library is not installed.";
    status_ = Status::kNotInstalled;
    return;
  }

  // Add dlopen flags (RTLD_LAZY | RTLD_NODELETE) later.
  base::ScopedNativeLibrary library = base::ScopedNativeLibrary(path);
  if (!library.is_valid()) {
    LOG(ERROR) << "Failed to load decoder shared library from: " << path
               << ", error: " << library.GetError()->ToString();
    status_ = Status::kLoadLibraryFailed;
    return;
  }

  // TODO(b/172527471): Remove it when decoder DSO is uprevved.
  createMainEntry_ = reinterpret_cast<ImeMainEntryCreateFn>(
      library.GetFunctionPointer(IME_MAIN_ENTRY_CREATE_FN_NAME));

  // TODO(b/172527471): Create a macro to fetch function pointers.
  entry_points_.initOnce = reinterpret_cast<ImeDecoderInitOnceFn>(
      library.GetFunctionPointer("ImeDecoderInitOnce"));
  entry_points_.support = reinterpret_cast<ImeDecoderSupportsFn>(
      library.GetFunctionPointer("ImeDecoderSupports"));
  entry_points_.activateIme = reinterpret_cast<ImeDecoderActivateImeFn>(
      library.GetFunctionPointer("ImeDecoderActivateIme"));
  entry_points_.process = reinterpret_cast<ImeDecoderProcessFn>(
      library.GetFunctionPointer("ImeDecoderProcess"));
  entry_points_.closeDecoder = reinterpret_cast<ImeDecoderCloseFn>(
      library.GetFunctionPointer("ImeDecoderClose"));
  if (!isEntryPointsLoaded(entry_points_)) {
    status_ = Status::kFunctionMissing;
    return;
  }
  entry_points_.isReady = true;

  // Optional function pointer.
  ImeEngineLoggerSetterFn loggerSetter =
      reinterpret_cast<ImeEngineLoggerSetterFn>(
          library.GetFunctionPointer("SetImeEngineLogger"));
  if (loggerSetter) {
    loggerSetter(ImeLoggerBridge);
  } else {
    LOG(WARNING) << "Failed to set a Chrome Logger for decoder DSO.";
  }

  library_ = std::move(library);
  status_ = Status::kSuccess;
}

ImeDecoder::~ImeDecoder() = default;

ImeDecoder* ImeDecoder::GetInstance() {
  static base::NoDestructor<ImeDecoder> instance;
  return instance.get();
}

ImeDecoder::Status ImeDecoder::GetStatus() const {
  return status_;
}

// TODO(b/172527471): Remove it when decoder DSO is uprevved.
ImeEngineMainEntry* ImeDecoder::CreateMainEntry(ImeCrosPlatform* platform) {
  DCHECK(createMainEntry_);
  return createMainEntry_(platform);
}

ImeDecoder::EntryPoints ImeDecoder::GetEntryPoints() {
  DCHECK(status_ == Status::kSuccess);
  return entry_points_;
}

}  // namespace ime
}  // namespace chromeos
