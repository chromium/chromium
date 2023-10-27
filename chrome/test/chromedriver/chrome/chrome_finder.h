// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_FINDER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_FINDER_H_

#include <vector>

#include "base/functional/callback_forward.h"

namespace base {
class FilePath;
}

// Gets the path to the default Chrome or Headless Shell executable.
// Supported |browser_name| is "chrome" or "chrome-headless-shell".
// If |browser_name| is different the value defaults to "chrome", a
// corresponding warning is issued.
// Returns true on success.
bool FindBrowser(const std::string& browser_name, base::FilePath& browser_exe);

// The overload for testing purposes
bool FindBrowser(
    const std::string& browser_name,
    const base::RepeatingCallback<bool(const base::FilePath&)>& exists_func,
    base::FilePath& browser_exe);

namespace internal {

bool FindExe(
    const base::RepeatingCallback<bool(const base::FilePath&)>& exists_func,
    const std::vector<base::FilePath>& rel_paths,
    const std::vector<base::FilePath>& locations,
    base::FilePath& out_path);

}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_FINDER_H_
