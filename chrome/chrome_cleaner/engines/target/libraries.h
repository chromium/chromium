// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_LIBRARIES_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_LIBRARIES_H_

#include <set>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"

namespace chrome_cleaner {

namespace internal {

using LibraryPostExtractionCallback =
    base::RepeatingCallback<void(const base::FilePath& dll_directory)>;

// Registers a callback that will be called after extracting the libraries in
// unit tests. This lets the tests mess with the DLL's before they're
// validated.
void SetLibraryPostExtractionCallbackForTesting(
    LibraryPostExtractionCallback post_extraction_callback);

// Clears a callback that was registered with
// SetLibraryPostExtractionCallbackForTesting.
void ClearLibraryPostExtractionCallbackForTesting();

}  // namespace internal

// Extracts the embedded libraries to |extraction_dir| and preloads them.
// Returns true if all libraries used by |engine| were validated as properly
// signed and were loaded.
bool LoadAndValidateLibraries(Engine::Name engine,
                              const base::FilePath& extraction_dir);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_TARGET_LIBRARIES_H_
