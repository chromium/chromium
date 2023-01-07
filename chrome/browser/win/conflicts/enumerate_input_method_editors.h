// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_ENUMERATE_INPUT_METHOD_EDITORS_H_
#define CHROME_BROWSER_WIN_CONFLICTS_ENUMERATE_INPUT_METHOD_EDITORS_H_

#include <stdint.h>

#include "base/functional/callback_forward.h"

namespace base {
class FilePath;
}

// The path to the registry key where IMEs are registered.
extern const wchar_t kImeRegistryKey[];

// Finds third-party IMEs (Input Method Editor) installed on the computer by
// enumerating the registry. In addition to the file path, the SizeOfImage and
// TimeDateStamp of the module is returned via the |on_ime_enumerated| callback.
using OnImeEnumeratedCallback =
    base::RepeatingCallback<void(const base::FilePath&, uint32_t, uint32_t)>;
void EnumerateInputMethodEditors(OnImeEnumeratedCallback on_ime_enumerated,
                                 base::OnceClosure on_enumeration_finished);

#endif  // CHROME_BROWSER_WIN_CONFLICTS_ENUMERATE_INPUT_METHOD_EDITORS_H_
