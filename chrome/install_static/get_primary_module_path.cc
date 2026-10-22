// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/get_primary_module_path.h"

#include <stddef.h>

#include <array>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "base/win/windows_types.h"

// A function exported by the primary module, which is expected to be built with
// the "primary_module" source set.
extern "C" size_t __declspec(dllimport) GetModulePath(wchar_t* buffer,
                                                      size_t buffer_size);

namespace install_static {

base::FilePath GetPrimaryModulePath() {
  STACK_UNINITIALIZED std::array<wchar_t, MAX_PATH> buffer;

  size_t path_len = GetModulePath(buffer.data(), buffer.size());
  return base::FilePath(
      base::as_string_view(base::span(buffer).first(path_len)));
}

}  // namespace install_static
