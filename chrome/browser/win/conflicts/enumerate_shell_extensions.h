// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_ENUMERATE_SHELL_EXTENSIONS_H_
#define CHROME_BROWSER_WIN_CONFLICTS_ENUMERATE_SHELL_EXTENSIONS_H_

#include <stdint.h>

#include "base/functional/callback_forward.h"

namespace base {
class FilePath;
}

// The path to the registry key where shell extensions are registered.
extern const wchar_t kApprovedShellExtensionRegistryKey[];

// Finds shell extensions installed on the computer by enumerating the registry.
// In addition to the file path, the SizeOfImage and TimeDateStamp of the module
// is returned via the |on_shell_extension_enumerated| callback.
using OnShellExtensionEnumeratedCallback =
    base::RepeatingCallback<void(const base::FilePath&, uint32_t, uint32_t)>;
void EnumerateShellExtensions(
    OnShellExtensionEnumeratedCallback on_shell_extension_enumerated,
    base::OnceClosure on_enumeration_finished);

namespace internal {

// Enumerates registered shell extensions, and invokes |callback| once per shell
// extension found. Must be called on a blocking sequence.
// Exposed for testing.
void EnumerateShellExtensionPaths(
    const base::RepeatingCallback<void(const base::FilePath&)>& callback);

}  // namespace internal

#endif  // CHROME_BROWSER_WIN_CONFLICTS_ENUMERATE_SHELL_EXTENSIONS_H_
