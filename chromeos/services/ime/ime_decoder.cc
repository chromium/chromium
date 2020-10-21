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

  createMainEntry_ = reinterpret_cast<ImeMainEntryCreateFn>(
      library.GetFunctionPointer(IME_MAIN_ENTRY_CREATE_FN_NAME));
  if (!createMainEntry_) {
    status_ = Status::kFunctionMissing;
    return;
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

ImeEngineMainEntry* ImeDecoder::CreateMainEntry(ImeCrosPlatform* platform) {
  DCHECK(status_ == Status::kSuccess);
  return createMainEntry_(platform);
}

}  // namespace ime
}  // namespace chromeos
