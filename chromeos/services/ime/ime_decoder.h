// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_IME_DECODER_H_
#define CHROMEOS_SERVICES_IME_IME_DECODER_H_

#include "chromeos/services/ime/public/cpp/shared_lib/interfaces.h"

#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/scoped_native_library.h"

namespace chromeos {
namespace ime {

// A proxy class for the IME decoder.
// ImeDecoder is implemented as a singleton and is initialized before 'ime'
// sandbox is engaged.
class ImeDecoder {
 public:
  // Status of loading func from IME decoder DSO: either success or error type.
  enum class Status {
    kSuccess = 0,
    kUninitialized = 1,
    kNotInstalled = 2,
    kLoadLibraryFailed = 3,
    kFunctionMissing = 4,
  };

  // Gets the singleton ImeDecoder.
  static ImeDecoder* GetInstance();

  // Get status of the IME decoder library initialization.
  // Return `Status::kSuccess` if the lib is successfully initialized.
  Status GetStatus() const;

  // Returns an instance of ImeEngineMainEntry from the IME shared library.
  ImeEngineMainEntry* CreateMainEntry(ImeCrosPlatform* platform);

 private:
  friend class base::NoDestructor<ImeDecoder>;

  // Initialize the Ime decoder library.
  explicit ImeDecoder();
  ~ImeDecoder();

  Status status_;

  // Result of IME decoder DSO initialization.
  base::Optional<base::ScopedNativeLibrary> library_;
  ImeMainEntryCreateFn createMainEntry_;

  DISALLOW_COPY_AND_ASSIGN(ImeDecoder);
};

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_IME_DECODER_H_
