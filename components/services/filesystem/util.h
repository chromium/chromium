// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FILESYSTEM_UTIL_H_
#define COMPONENTS_SERVICES_FILESYSTEM_UTIL_H_

#include <stdint.h>

#include <string>

#include "base/files/file.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"

namespace filesystem {

// Validation functions (typically used to check arguments; they return
// |ERROR_OK| if valid, else the standard/recommended error for the validation
// error):

// Checks if |path| is (looks like) a valid (relative) path. (On failure,
// returns |ERROR_INVALID_ARGUMENT| if |path| is not UTF-8, or
// |ERROR_PERMISSION_DENIED| if it is not relative.)
base::File::Error IsPathValid(const std::string& path);

// Checks if |whence| is a valid (known) |Whence| value. (On failure, returns
// |ERROR_UNIMPLEMENTED|.)
base::File::Error IsWhenceValid(mojom::Whence whence);

// Checks if |offset| is a valid file offset (from some point); this is
// implementation-dependent (typically checking if |offset| fits in an |off_t|).
// (On failure, returns |ERROR_OUT_OF_RANGE|.)
base::File::Error IsOffsetValid(int64_t offset);

// Conversion functions:

// Returns the platform dependent error details converted to the
// filesystem.Error enum.
base::File::Error GetError(const base::File& file);

// Serializes Info to the wire format.
mojom::FileInformationPtr MakeFileInformation(const base::File::Info& info);

// Creates an absolute file path and ensures that we don't try to traverse up.
base::File::Error ValidatePath(const std::string& raw_path,
                               const base::FilePath& filesystem_base,
                               base::FilePath* out);

}  // namespace filesystem

#endif  // COMPONENTS_SERVICES_FILESYSTEM_UTIL_H_
