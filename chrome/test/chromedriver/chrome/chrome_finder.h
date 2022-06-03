// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_FINDER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_FINDER_H_

#include <vector>

#include "base/callback_forward.h"

namespace base {
class FilePath;
}

// Gets the path to the default Chrome executable. Returns true on success.
bool FindChrome(base::FilePath* browser_exe);

namespace internal {

bool FindExe(
    const base::RepeatingCallback<bool(const base::FilePath&)>& exists_func,
    const std::vector<base::FilePath>& rel_paths,
    const std::vector<base::FilePath>& locations,
    base::FilePath* out_path);

}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_FINDER_H_
