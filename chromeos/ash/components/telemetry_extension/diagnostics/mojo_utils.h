// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_DIAGNOSTICS_MOJO_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_DIAGNOSTICS_MOJO_UTILS_H_

#include <string>

#include "mojo/public/cpp/system/handle.h"

namespace ash::converters::diagnostics {

// This class is created to enable its functions (i.e.
// GetStringPieceFromMojoHandle) to use base::ScopedAllowBlocking's private
// constructor. That is achieved by making this class friend of
// base::ScopedAllowBlocking.
// Non-goal: This class is not created to group static member functions (as
// discouraged by chromium style guide).
class MojoUtils final {
 public:
  // Disallow all implicit constructors.
  MojoUtils() = delete;
  MojoUtils(const MojoUtils& mojo_utils) = delete;
  MojoUtils& operator=(const MojoUtils& mojo_utils) = delete;

  // Allows to get access to the buffer in read only shared memory. It converts
  // mojo::Handle to base::ReadOnlySharedMemoryMapping and returns a string.
  // Returns an empty string if error.
  //
  // |handle| must be a valid mojo handle of the non-empty buffer in the shared
  // memory.
  static std::string GetStringFromMojoHandle(mojo::ScopedHandle handle);
};

}  // namespace ash::converters::diagnostics

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_DIAGNOSTICS_MOJO_UTILS_H_
