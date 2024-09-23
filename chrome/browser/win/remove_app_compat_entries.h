// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_REMOVE_APP_COMPAT_ENTRIES_H_
#define CHROME_BROWSER_WIN_REMOVE_APP_COMPAT_ENTRIES_H_

#include <string>

namespace base {
class FilePath;
}

// Removes the appcompat layers for `program`, which must be a fully-qualified
// path to an executable. In this context, a layer is the symbolic name of a
// compatibility mode that is applied to an executable a runtime by
// AcLayers.dll. These are intended to be used to allow programs written for an
// older version of Windows to run on a newer version. They should never be used
// on Chrome.
void RemoveAppCompatEntries(const base::FilePath& program);

// Removes the compatibility mode layers from the string `layers`. Returns true
// if `layers` was modified.
bool RemoveCompatLayers(std::wstring& layers);

#endif  // CHROME_BROWSER_WIN_REMOVE_APP_COMPAT_ENTRIES_H_
